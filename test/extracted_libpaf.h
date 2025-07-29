#ifndef LIBPAF_H
#define LIBPAF_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* path;      // UTF-8�p�X�iNULL�I�[�j
    uint32_t size;   // �t�@�C���T�C�Y
    uint32_t offset; // �f�[�^�u���b�N���̃I�t�Z�b�g
} PafEntry;

typedef struct {
    PafEntry* entries; // �t�@�C���ꗗ
    uint32_t count;    // �t�@�C����
} PafList;

//// API ////

// �A�[�J�C�u���쐬�ipaths: UTF-8�̃t�@�C��/�t�H���_�p�X�̔z��j
int paf_create(const char* out_paf_path, const char** paths, int path_count);

// �ꗗ���擾�i�Ăяo������ free_paf_list() �K�{�j
int paf_list(const char* paf_path, PafList* out_list);

// �P��t�@�C���𒊏o�i�t�@�C�����̓��X�g�Ŏ擾�ł���j
int paf_extract_file(const char* paf_path, const char* file_name, const char* out_path);

// �S�t�@�C���𒊏o�i�o�̓t�H���_�ɓW�J�j
int paf_extract_all(const char* paf_path, const char* output_dir);

// ��n��
void free_paf_list(PafList* list);

#ifdef __cplusplus
}
#endif

#endif // LIBPAF_H
