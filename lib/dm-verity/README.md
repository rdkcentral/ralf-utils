# DmVerity

This is a collection of classes and functions for working with dm-verity hash trees.

## Links
* [Implementing dm-verity](https://source.android.com/docs/security/features/verifiedboot/dm-verity)
* [An Introduction to Dm-verity in Embedded Device Security](https://www.starlab.io/blog/dm-verity-in-embedded-device-security)
* [Linux Kernel - dm-verity](https://docs.kernel.org/admin-guide/device-mapper/verity.html)


## Classes

### DmVerityMounter
Helper class with static methods to map and mount a file system image with
dm-verity protection.

It's intended to be used for directly mounting EROFS formatted widgets.

### IDmVerityVerifier
Interface and implementation of a userspace dm-verity verifier.  It can load
a dm-verity formatted merkle hash tree and can verify data blocks against that
hash tree.

It's intended to be used for verifying packages when fully extracting them in
userspace rather than mounting.

### DmVerityProtectedFile
Wraps a file and implements the `IDmVerityProtectedFile` interface, it supports
random reads of the underlying file, but also verifies the data read with an
internal `IDmVerityVerifier` instance.  Any data read from this object is
therefore verified against the hash tree.
