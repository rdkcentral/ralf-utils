# Erofs Component

## EROFS - Enhanced Read Only Filesystem
EROFS is a read-only linux file system, the intention is to use it as the archive
format for widgets instead of the current zip format.  The advantage of using
EROFS is the widgets can be directly mounted by the kernel and used, instead
of requiring us to extract them and then run.

Useful links:
* https://erofs.docs.kernel.org/en/latest/index.html
* https://docs.kernel.org/filesystems/erofs.html
* https://source.android.com/docs/core/architecture/kernel/erofs
* https://github.com/erofs/erofs-utils


## IErofsImageFile
Interface used for reading the raw data of an EROFS image.  This is typically
used to implement dm-verity protection for all file reads.  `IErofsReader`
can use an implementation of `IErofsImageFile` for reading an image.


## IErofsReader
Class object that allows for reading a EROFS image and extracting files from it.
A typical usage:
```c++
IErofsReader *reader = IErofsReader::create("some.erofs.img");
while (reader->hasNext())
{
    std::unique_ptr<IErofsEntry> entry = reader.next();
    std::cout << "path:" << entry->path() << " type:" << entry->type() << std::endl;

    // to read the file
    if (entry->type() == std::filesystem::file_type::regular_file)
    {
        entry->read(...);
    }
}

if (reader->hasError())
{
    std::cerr << "failed to read erofs image due to " << reader->errorString() << std::endl;
}
```

`IErofsReader` doesn't provide methods to extract a single file, instead you
need to iterate through the entire image to find a single file, however the
actual data is not read from the image until `IErofsEntry::read()` is called,
so it's fast and efficient to iterate search the image this way.

The order of entries is not fixed, however you are guaranteed to receive the
parent directory entry before any children of the directory.


## IErofsEntry
Object that provides details on an entry in the EROFS image.  An entry can be
either a directory, regular file or a symlink, you should use `IErofsEntry::type()`
to get the entry type.

`IErofsEntry::path()` returns the full path of the entry within the EROFS image
without a leading slash (`\`).

`IErofsEntry::read()` will read the contents of a regular file, for a symlink
this will return the target path of the symlink.  Calling IErofsEntry::read()`
on a directory entry will return an error.

