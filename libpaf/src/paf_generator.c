#define LIBPAF_EXPORTS
#include "paf_generator.h"
#include "paf_gpu_loader.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(__ANDROID__) && !defined(__linux__)
#include <windows.h>
#else
#include <unistd.h>
#define _strdup strdup
#endif

int paf_generator_init(paf_generator_t* gen) {
    if (!gen) return -1;
    gen->data_tmp = tmpfile();
    gen->index_tmp = tmpfile();
    gen->path_tmp = tmpfile();
    gen->current_data_offset = 0;
    gen->current_path_offset = 0;
    gen->file_count = 0;
    gen->batch_count = 0;
    gen->batch_buffer_pos = 0;
    gen->index_only = 0;
    gen->batch_data_buffer = (uint8_t*)malloc(128 * 1024 * 1024);
    gen->batch_sizes = (uint64_t*)malloc(sizeof(uint64_t) * 4096);
    gen->batch_offsets = (uint64_t*)malloc(sizeof(uint64_t) * 4096);
    gen->batch_paths = (char**)malloc(sizeof(char*) * 4096);

    if (!gen->data_tmp || !gen->index_tmp || !gen->path_tmp ||
        !gen->batch_data_buffer || !gen->batch_sizes || !gen->batch_offsets || !gen->batch_paths) {
        paf_generator_cleanup(gen);
        return -1;
    }
    return 0;
}

static int paf_generator_flush_batch(paf_generator_t* gen) {
    if (gen->batch_count == 0) return 0;

    uint8_t* host_hashes = (uint8_t*)malloc(gen->batch_count * 32);
    if (!host_hashes) return -1;
    
    // GPU path: CUDA → Vulkan → CPU fallback
    int gpu_done = (paf_cuda_is_available() && g_paf_cuda_hash_flat != NULL &&
                    g_paf_cuda_hash_flat(gen->batch_data_buffer, gen->batch_offsets,
                                         gen->batch_sizes, gen->batch_count, host_hashes) == 0)
                || (paf_vulkan_is_available() && g_paf_vulkan_hash_flat != NULL &&
                    g_paf_vulkan_hash_flat(gen->batch_data_buffer, gen->batch_offsets,
                                           gen->batch_sizes, gen->batch_count, host_hashes) == 0);
    if (!gpu_done) {
        for (uint32_t i = 0; i < gen->batch_count; i++) {
            sha256_context_t sha_ctx;
            sha256_init(&sha_ctx);
            sha256_update(&sha_ctx, gen->batch_data_buffer + gen->batch_offsets[i], (size_t)gen->batch_sizes[i]);
            sha256_final(&sha_ctx, host_hashes + (i * 32));
        }
    }

    // Step 3: Process Index
    for (uint32_t i = 0; i < gen->batch_count; i++) {
        paf_index_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.path_buffer_offset = gen->current_path_offset;
        entry.path_length = (uint32_t)strlen(gen->batch_paths[i]);
        entry.data_size = gen->batch_sizes[i];
        memcpy(entry.hash, host_hashes + (i * 32), 32);

        entry.data_offset = gen->current_data_offset;
        if (!gen->index_only) {
            if (fwrite(gen->batch_data_buffer + gen->batch_offsets[i], 1, (size_t)gen->batch_sizes[i], gen->data_tmp) != (size_t)gen->batch_sizes[i]) return -1;
            gen->current_data_offset += gen->batch_sizes[i];
        }

        if (fwrite(gen->batch_paths[i], 1, entry.path_length, gen->path_tmp) != entry.path_length) return -1;
        if (fwrite(&entry, sizeof(entry), 1, gen->index_tmp) != 1) return -1;

        gen->current_path_offset += entry.path_length;
        gen->file_count++;
        free(gen->batch_paths[i]);
    }

    free(host_hashes);
    gen->batch_count = 0;
    gen->batch_buffer_pos = 0;
    return 0;
}

int paf_generator_add_file(paf_generator_t* gen, const char* path, const uint8_t* data, uint64_t size) {
    if (!gen || !path || !data) return -1;
    if (strlen(path) >= 1024) return -1;

    // Safety: If this file is too large for the buffer, or the buffer is full
    if (gen->batch_buffer_pos + size > 128 * 1024 * 1024 || gen->batch_count >= 4096) {
        int res = paf_generator_flush_batch(gen);
        if (res != 0) return res;
        
        // If the single file is STILL larger than 128MB, process it as its own special batch
        if (size > 128 * 1024 * 1024) {
            paf_index_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            entry.path_buffer_offset = gen->current_path_offset;
            entry.path_length = (uint32_t)strlen(path);
            entry.data_size = size;

            {
                uint64_t offset_val = 0;
                int gpu_done = paf_cuda_is_available() && g_paf_cuda_hash_flat != NULL &&
                               g_paf_cuda_hash_flat(data, &offset_val, &size, 1, entry.hash) == 0;
                if (!gpu_done) {
                    sha256_context_t sha_ctx;
                    sha256_init(&sha_ctx);
                    sha256_update(&sha_ctx, data, (size_t)size);
                    sha256_final(&sha_ctx, entry.hash);
                }
            }
            
            entry.data_offset = gen->current_data_offset;
            if (!gen->index_only) {
                fwrite(data, 1, (size_t)size, gen->data_tmp);
                gen->current_data_offset += size;
            }
            
            fwrite(path, 1, entry.path_length, gen->path_tmp);
            fwrite(&entry, sizeof(entry), 1, gen->index_tmp);
            
            gen->current_path_offset += entry.path_length;
            gen->file_count++;
            
            return 0;
        }
    }

    // Buffer the file without malloc
    memcpy(gen->batch_data_buffer + gen->batch_buffer_pos, data, (size_t)size);
    gen->batch_offsets[gen->batch_count] = gen->batch_buffer_pos;
    gen->batch_sizes[gen->batch_count] = size;
    gen->batch_paths[gen->batch_count] = _strdup(path);
    if (!gen->batch_paths[gen->batch_count]) return -1;
    
    gen->batch_buffer_pos += size;
    gen->batch_count++;

    return 0;
}

int paf_generator_add_file_stream(paf_generator_t* gen, const char* path, FILE* fp, uint64_t size) {
    if (!gen || !path || !fp) return -1;
    if (strlen(path) >= 1024) return -1;

    if (paf_generator_flush_batch(gen) != 0) return -1;

    sha256_context_t ctx;
    sha256_init(&ctx);
    uint8_t chunk[65536];
    uint64_t remaining = size;

    while (remaining > 0) {
        size_t to_read = (remaining > sizeof(chunk)) ? sizeof(chunk) : (size_t)remaining;
        size_t n = fread(chunk, 1, to_read, fp);
        if (n == 0) break;
        sha256_update(&ctx, chunk, n);
        if (!gen->index_only) {
            if (fwrite(chunk, 1, n, gen->data_tmp) != n) return -1;
        }
        remaining -= n;
    }

    paf_index_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.path_buffer_offset = gen->current_path_offset;
    entry.path_length        = (uint32_t)strlen(path);
    entry.data_size          = size;
    entry.data_offset        = gen->current_data_offset;
    sha256_final(&ctx, entry.hash);

    if (!gen->index_only) gen->current_data_offset += size;

    if (fwrite(path, 1, entry.path_length, gen->path_tmp) != entry.path_length) return -1;
    if (fwrite(&entry, sizeof(entry), 1, gen->index_tmp) != 1) return -1;
    gen->current_path_offset += entry.path_length;
    gen->file_count++;
    return 0;
}

static int copy_file_stream(FILE* src, FILE* dst) {
    char buffer[65536];
    size_t n;
    fseek(src, 0, SEEK_SET);
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, n, dst) != n) return -1;
    }
    return 0;
}

int paf_generator_finalize(paf_generator_t* gen, const char* output_path) {
    if (!gen || !output_path) return -1;

    // Flush any remaining files in the buffer
    if (paf_generator_flush_batch(gen) != 0) return -1;

    FILE* out = fopen(output_path, "wb");
    if (!out) return -1;

    paf_header_t header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, PAF_MAGIC, 4);
    header.version = PAF_VERSION;
    header.file_count = gen->file_count;

    uint64_t header_size = sizeof(paf_header_t);
    uint64_t index_size = (uint64_t)gen->file_count * sizeof(paf_index_entry_t);

    if (gen->index_only) {
        header.flags = PAF_FLAG_INDEX_ONLY;
        header.index_offset = header_size;
        header.path_offset  = header_size + index_size;
    } else {
        uint64_t data_size = gen->current_data_offset;
        header.index_offset = header_size + data_size;
        header.path_offset  = header.index_offset + index_size;
    }

    if (fwrite(&header, sizeof(header), 1, out) != 1) {
        fclose(out);
        return -1;
    }

    if (!gen->index_only && copy_file_stream(gen->data_tmp, out) != 0) {
        fclose(out);
        return -1;
    }

    if (copy_file_stream(gen->index_tmp, out) != 0) {
        fclose(out);
        return -1;
    }

    if (copy_file_stream(gen->path_tmp, out) != 0) {
        fclose(out);
        return -1;
    }

    fclose(out);
    return 0;
}

void paf_generator_cleanup(paf_generator_t* gen) {
    if (!gen) return;
    if (gen->data_tmp) fclose(gen->data_tmp);
    if (gen->index_tmp) fclose(gen->index_tmp);
    if (gen->path_tmp) fclose(gen->path_tmp);
    if (gen->batch_data_buffer) free(gen->batch_data_buffer);
    if (gen->batch_sizes) free(gen->batch_sizes);
    if (gen->batch_offsets) free(gen->batch_offsets);
    if (gen->batch_paths) {
        for(uint32_t i=0; i<gen->batch_count; i++) free(gen->batch_paths[i]);
        free(gen->batch_paths);
    }
    gen->data_tmp = NULL;
    gen->index_tmp = NULL;
    gen->path_tmp = NULL;
}
