# lustre-hsm-restore
Small utility to trigger restores of Lustre HSM released files

Traverses every directory and file on the provided path as the first parameter (or the current directory if none provided), and will trigger a synchronous restore of every HSM released Lustre file found.

Errors are reported to stderr, including files that have been "lost", all others operations are reported to stdout.
