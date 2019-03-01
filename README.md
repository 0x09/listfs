listfs - Barebones FUSE driver for presenting a list of existing filesystem objects

listfs mounts a text listing of absolute paths as a filesystem hierarchy. This can be handy for using a file manager interface to view a curated set of paths, beyond the abilities of the FM's built in search.
In effect, the intent is a more controlled "Spotlight window".

Regular read, stat, and other non-directory requests are simply passed directly through to the OS.

## Example

Create a directory tree under /mnt/listfs showing all files that reference fuse.h in ~/Development

	fgrep -wlR fuse.h ~/Development | listfs /dev/stdin /mnt/listfs

For instance, $HOME/Development/listfs/src/listfs.c would appear in /mnt/listfs/$HOME/Development/listfs/src, but the Makefile won't

## Building
listfs is compatible with most FUSE implementations, and builds on macOS, Linux, and FreeBSD

    make
    make install

*gmake on FreeBSD

## Use
    listfs <opts> <file> <mountpoint>

Where `<opts>` are any series of arguments to be passed along to FUSE. Use `listfs -h` for a list of switches.
