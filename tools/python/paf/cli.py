# cli.py
import argparse
from paf.core import create_paf, extract_paf, list_paf, to_iso

def main():
    parser = argparse.ArgumentParser(prog="paf", description="Parallel Archive Format CLI")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # create
    p_create = subparsers.add_parser("create", help="Create a PAF archive")
    p_create.add_argument("archive", help="Output .paf file")
    p_create.add_argument("source", help="Input file or directory")

    # extract
    p_extract = subparsers.add_parser("extract", help="Extract a PAF archive")
    p_extract.add_argument("archive", help="Input .paf file")
    p_extract.add_argument("destination", help="Destination directory")

    # ls
    p_ls = subparsers.add_parser("ls", help="List contents of a PAF archive")
    p_ls.add_argument("archive", help="Input .paf file")

    # to-iso
    p_iso = subparsers.add_parser("to-iso", help="Convert a PAF archive to ISO")
    p_iso.add_argument("archive", help="Input .paf file")
    p_iso.add_argument("output", help="Output .iso file")

    args = parser.parse_args()

    if args.command == "create":
        create_paf(args.archive, [args.source])
    elif args.command == "extract":
        extract_paf(args.archive, args.destination)
    elif args.command == "ls":
        list_paf(args.archive)
    elif args.command == "to-iso":
        to_iso(args.archive, args.output)
