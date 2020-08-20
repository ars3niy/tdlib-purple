#!/usr/bin/env python3

# Yes, this program *is* overkill.
# However, it solves all of the following problems:
# - If a translator accidentally messes up and assigns conflicting accelerators, then users don't have to suffer.
# - If by random chance two accelerators share a letter, but don't appear together in any dialog, then don't complain.
# - Nice error reporting if *anything* goes wrong. (Try doing that in /bin/sh!)

from collections import defaultdict
import sys
import os

CONFLICT_PAIRS = [
    ('_OK', '_Cancel'),
    ('_Accept', '_Cancel'),
    ('_Yes', '_No'),
]

MSGIDS = {e for pair in CONFLICT_PAIRS for e in pair}


def should_ignore_missing(_magic = []):
    envvar = os.environ.get('LINTACCEL_IGNORE_MISSING')
    if envvar is None:
        return False
    if envvar.lower() == 'n' or envvar == '0':
        return False
    if envvar.lower() == 'y' or envvar == '1' or envvar == '':
        return True
    if not _magic:
        print('[WARNING: ambiguous environment variable LINTACCEL_IGNORE_MISSING: "{}". Should be one of (unset), empty, "0", "1", "y", "n", "Y", or "N".]'.format(envvar))
        _magic.append(True)
    # Be on the safe side.
    return False


def read_pofile_values(pofile):
    last_msgid = None
    msgid_to_msgstr = dict()
    with open(pofile) as fp:
        for n, line in enumerate(fp.readlines()):
            line = line.rstrip()
            if line.startswith('msgid'):
                if last_msgid is not None:
                    print('Duplicate msgid around line {}'.format(n))
                    return None
                if not line.startswith('msgid "') or not line.endswith('"'):
                    print('Weird msgid around line {}'.format(n))
                    return None
                last_msgid = line[len('msgid "'):-len('"')]
                continue
            if not line.startswith('msgstr'):
                last_msgid = None
                continue
            if last_msgid in MSGIDS:
                if not line.startswith('msgstr "') or not line.endswith('"'):
                    print('Weird msgstr around line {}'.format(n))
                    return None
                msgstr = line[len('msgstr "'):-len('"')]
                if last_msgid in msgid_to_msgstr.keys():
                    print('Redefinition of msgid {} around line {} (old: "{}", new: {})'.format(last_msgid, n, msgid_to_msgstr[last_msgid], msgstr))
                    return None
                msgid_to_msgstr[last_msgid] = msgstr

            last_msgid = None

    return msgid_to_msgstr


def extract_accel(msgstr):
    parts = msgstr.split('_')
    if len(parts) != 2:
        print('msgstr "{}" should have exactly one underscore'.format(msgstr))
        return None
    if len(parts[1]) == 0:
        print('msgstr "{}" has one underscore, but no accelerator after it?!'.format(msgstr))
        return None
    return parts[1][0]


def check_pofile(pofile):
    msgid_to_msgstr = read_pofile_values(pofile)
    if msgid_to_msgstr is None:
        print('Could not read {}'.format(pofile))
        return 2

    missing_msgids = MSGIDS.difference(msgid_to_msgstr.keys())
    if missing_msgids:
        print('Missing translations for {}'.format(missing_msgids))
        if not should_ignore_missing():
            return 2
        else:
            print('        in PO-file {}'.format(pofile))

    return_code = 0
    msgid_to_accel = dict()
    for msgid, msgstr in msgid_to_msgstr.items():
        accel = extract_accel(msgstr)
        if accel is None:
            print('    for msgid {}'.format(msgid))
            accel = '_' + msgid  # Anything unique to avoid clashes and crashes.
            return_code = 1
        else:
            # Ideally we want to know the same character-to-key mapping that the frontend uses.
            # However, that is not available, and probably impossible. `lower()` has to suffice.
            accel = accel.lower()
        msgid_to_accel[msgid] = accel

    for a, b in CONFLICT_PAIRS:
        if a in msgid_to_accel and b in msgid_to_accel and msgid_to_accel[a] == msgid_to_accel[b]:
            return_code = 1
            print('Conflicting accels: "{}" ({}) and "{}" ({}) both use the key "{}", and will appear in the same dialog.'.format(
                msgid_to_msgstr[a], a, msgid_to_msgstr[b], b, msgid_to_accel[b]))

    return return_code


def get_relative_path(filename):
    own_dirname = os.path.dirname(__file__)
    return os.path.join(own_dirname, filename)


def run():
    exitcode = 0
    with open(get_relative_path('LINGUAS')) as fp:
        for lang in fp.readlines():
            lang = lang.strip()
            pofile = get_relative_path(lang + '.po')
            if not os.path.exists(pofile):
                exitcode = max(2, exitcode)
                print('PO-file {} does not exist?!'.format(pofile))
                continue
            pofile_code = check_pofile(pofile)
            if pofile_code > 0:
                print('        in PO-file {}'.format(pofile))
            exitcode = max(pofile_code, exitcode)
    return exitcode


if __name__ == '__main__':
    exit(run())
