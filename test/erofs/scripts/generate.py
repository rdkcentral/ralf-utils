#!/usr/bin/env python3
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import json
import stat
import time
import base64
import shutil
import random
import hashlib
import tempfile
import subprocess
from pathlib import Path


mkfsErofs="mkfs.erofs"


class ErofsImage:
    def __init__(self, compr = "lz4hc", legacy = False):
        self.compr = compr
        self.legacy = legacy
        self.preserve_mtime = False
        self.built_time = int(time.time())
        self.details = {}
        self.temp_dir = tempfile.TemporaryDirectory()


    def _storedetails(self, path, digest = ""):
        s = os.lstat(self.temp_dir.name + os.sep + path)

        self.details[path] = {}
        self.details[path]["size"] = s.st_size
        self.details[path]["mode"] = stat.S_IFMT(s.st_mode) | stat.S_IMODE(s.st_mode)
        self.details[path]["uid"] = 0
        self.details[path]["gid"] = 0
        if self.preserve_mtime:
            self.details[path]["mtime"] = int(s.st_mtime)
        else:
            self.details[path]["mtime"] = int(self.built_time)
        if not stat.S_ISDIR(s.st_mode):
            self.details[path]["sha256"] = digest



    def _createdirs(self, path, perms, mtime=None):
        if len(path) > 0:
            parts = path.split(os.sep)
            fullpath = ""

            for i in range(len(parts)):
                fullpath += parts[i]
                if not os.path.isdir(self.temp_dir.name + os.sep + fullpath):
                    os.mkdir(self.temp_dir.name + os.sep + fullpath, perms)
                    if mtime is not None:
                        os.utime(self.temp_dir.name + os.sep + fullpath, (mtime, mtime))
                    self._storedetails(fullpath)

                fullpath += os.sep


    def addzerofile(self, path, size, perms=0o644):
        head_tail = os.path.split(path)
        self._createdirs(head_tail[0], 0o755)

        with open(self.temp_dir.name + os.sep + path, 'wb') as outfile:

            # sha256 of the file
            h = hashlib.new('sha256')

            # write zeros into the file
            while size > 0:
                # read a chunk
                bufsize = min(4096, size)
                zerobuf = bytearray(bufsize)

                # add to the sha256 digest
                h.update(zerobuf)

                # write to the output file
                outfile.write(zerobuf)
                size -= bufsize

            outfile.close()

        # finally set the perms on the new file
        os.chmod(self.temp_dir.name + os.sep + path, perms)

        # update the details
        self._storedetails(path, h.hexdigest())


    def addfile(self, path, size, perms=0o644, mtime=None):
        head_tail = os.path.split(path)
        self._createdirs(head_tail[0], 0o755)

        outfile_path = Path(self.temp_dir.name + os.sep + path)
        while outfile_path.exists():
            path += "_"
            outfile_path = Path(self.temp_dir.name + os.sep + path)

        with open("sherlock-holmes.txt", 'rb') as infile, open(outfile_path, 'wb') as outfile:

            # seek to random location to read from
            infile.seek(0, os.SEEK_END)
            limit = infile.tell() - size
            infile.seek(random.randint(0, limit), os.SEEK_SET)

            # sha256 of the file
            h = hashlib.new('sha256')

            # copy from source to dest
            while size > 0:
                # read a chunk
                bufsize = min(1024, size)
                buf = infile.read(bufsize)

                # add to the sha256 digest
                h.update(buf)

                # write to the output file
                outfile.write(buf)
                size -= bufsize

            outfile.close()

            # set the mtime if needed
            if mtime is not None:
                os.utime(outfile_path, (mtime, mtime))

            # finally set the perms on the new file
            os.chmod(outfile_path, perms)

            # update the details
            self._storedetails(path, h.hexdigest())




    def addsymlink(self, path, target, mtime=None):
        head_tail = os.path.split(path)
        self._createdirs(head_tail[0], 0o755)

        # add the symlink
        abs_path = Path(self.temp_dir.name + os.sep + path)
        os.symlink(target, abs_path)

        # set the mtime if needed
        if mtime is not None:
            os.utime(abs_path, (mtime, mtime), follow_symlinks=False)

        # s = os.lstat(self.temp_dir.name + os.sep + path)

        # update the details
        self._storedetails(path, hashlib.sha256(target.encode('utf-8')).hexdigest())



    def adddir(self, path, perms=0o755, mtime=None):
        self._createdirs(path, perms, mtime)


    def write(self, outfile):
        args = [ mkfsErofs ]

        # default args; no-xattr, 4k blocks
        args += ["-x-1", "-b4096", "-C4096", "--all-root"]

        # set the compression type
        if self.compr != "none":
            args += ["-z" + self.compr]

        # check if legacy format
        if self.legacy:
            args += ["-E", "legacy-compress"]

        # check if we need to preserve mtime
        if self.preserve_mtime:
            args += ["--preserve-mtime"]
        else:
            args += [f"-T{self.built_time}", "--ignore-mtime"]

        # append image file and source directory
        args += [outfile, self.temp_dir.name]

        # run the mkfs.erofs command
        subprocess.run(args)


    def exportJson(self, outfile):
        # post process details for the mtime of directories
        if self.preserve_mtime:
            for path in self.details.keys():
                s = os.lstat(self.temp_dir.name + os.sep + path)
                if stat.S_ISDIR(s.st_mode):
                    self.details[path]["mtime"] = int(s.st_mtime)

        # write out the json file
        jsonobj = json.dumps(self.details, indent=4)
        with open(outfile, "w") as outfile:
            outfile.write(jsonobj)


def gen_random_filename():
    random_bytes = os.urandom(random.randint(1, 96))
    base64_encoded = base64.b64encode(random_bytes).decode('utf-8')
    base64_encoded = base64_encoded.replace('/', '_')
    return base64_encoded


def create_empty_image():
    image = ErofsImage(compr="lz4hc")
    image.write("erofs_empty.img")
    image.exportJson("erofs_empty.json")


def create_image1():
    image = ErofsImage(compr="lz4hc")
    image.addfile("1b.txt", 1, 0o644)
    image.addfile("2b.txt", 2, 0o644)
    image.addfile("somedir/really/really/really/really/really/deep/4095.txt", 4095)
    image.addfile("somedir/really/really/really/no/really/really/deep/4096.txt", 4096)
    image.addfile("closer/to/home/4097.txt", 4097)
    image.addfile("big/big.txt", 1048576)
    image.addfile("big/big_1.txt", (1048576 * 3) - 4095)
    image.addfile("big/big_2.txt", (1048576 * 3) - 4097)
    image.addsymlink("im a link", "/to/usr/home")
    image.adddir("empty %$%^$%^/f&_/directory", 0o766)

    image.write("erofs_1.img")
    image.exportJson("erofs_1.json")


def create_image2():
    image = ErofsImage(compr="lz4hc")
    image.addzerofile("big_zero.bin", (30 * 1048576), 0o440)
    image.addzerofile("small_zero.bin", (1 * 1048576), 0o646)

    image.write("erofs_2.img")
    image.exportJson("erofs_2.json")


def create_legacy_image():
    image = ErofsImage(compr="lz4hc", legacy=True)
    image.addfile("1b.txt", 1, 0o644)
    image.addfile("2b.txt", 2, 0o644)
    image.addfile("somedir/really/really/really/really/really/deep/4095.txt", 4095)
    image.addfile("somedir/really/really/really/no/really/really/deep/4096.txt", 4096)
    image.addfile("closer/to/home/4097.txt", 4097)
    image.addfile("big/big.txt", 1048576)
    image.addfile("big/big_1.txt", (1048576 * 3) - 4095)
    image.addfile("big/big_2.txt", (1048576 * 3) - 4097)
    image.addsymlink("im a link", "/to/usr/home")
    image.adddir("empty %$%^$%^/f&_/directory", 0o766)

    image.write("erofs_legacy.img")
    image.exportJson("erofs_legacy.json")


def create_uncompr_image():
    image = ErofsImage(compr="none")
    image.addfile("1b.txt", 1, 0o644)
    image.addfile("2b.txt", 2, 0o644)
    image.addfile("somedir/really/really/really/really/really/deep/4095.txt", 4095)
    image.addfile("somedir/really/really/really/no/really/really/deep/4096.txt", 4096)
    image.addfile("closer/to/home/4097.txt", 4097)
    image.addfile("big/big.txt", 1048576)
    image.addfile("big/big_1.txt", (1048576 * 3) - 4095)
    image.addfile("big/big_2.txt", (1048576 * 3) - 4097)
    image.addsymlink("im a link", "/to/usr/home")
    image.adddir("empty %$%^$%^/f&_/directory", 0o766)

    image.write("erofs_uncompr.img")
    image.exportJson("erofs_uncompr.json")


def create_lotsoffiles_image():
    image = ErofsImage(compr="none")
    n_files = 100000

    path = Path('')

    while n_files > 0:
        op = random.randint(0, 2)

        if op == 0:
            path = path / gen_random_filename()
            image.adddir(str(path), 0o766)
            n_files -= 1

        elif op == 1:
            if path != Path(''):
                path = path.parent

        else:
            files_in_dir = min(n_files, random.randint(0, 8192))
            for j in range(files_in_dir):
                image.addfile(str(path / gen_random_filename()), random.randint(1, 256), 0o644)
            n_files -= files_in_dir

    image.write("erofs_lotsoffiles.img")
    image.exportJson("erofs_lotsoffiles.json")

def create_utf8_fnames_image():
    image = ErofsImage(compr="lz4hc")
    image.addfile("普通话.txt", 1234, 0o644)
    image.addfile("Español.txt", 2345, 0o644)
    image.addfile("русский.txt", 3456, 0o644)
    image.addfile("العربية.txt", 4567, 0o644)
    image.addfile("हिन्दी.txt", 5678, 0o644)
    image.addsymlink("链接", "/to/目标")
    image.adddir("ディレクトリ", 0o755)

    image.write("erofs_utf8_fnames.img")
    image.exportJson("erofs_utf8_fnames.json")

def create_modtimes_image():
    image = ErofsImage(compr="lz4hc")
    image.preserve_mtime = True

    tp = int(time.time()) - 86400  # set mtime to 1 day ago

    image.addfile("one.txt", 1234, 0o644, mtime=(tp - 100))
    image.addfile("two.txt", 2345, 0o644, mtime=(tp - 200))
    image.addfile("three.txt", 3456, 0o644, mtime=(tp - 300))
    image.addsymlink("four.txt", "/three.txt", mtime=(tp - 400))
    image.adddir("foo", 0o755, mtime=(tp - 500))
    image.addfile("foo/bar.txt", 2468, 0o755, mtime=(tp - 123))

    image.write("erofs_mtimes.img")
    image.exportJson("erofs_mtimes.json")


if __name__ == '__main__':

    # move to the data directory
    os.chdir(os.path.dirname(os.path.realpath(__file__)) + '/../data/')

    create_empty_image()
    create_image1()
    create_image2()
    create_legacy_image()
    create_uncompr_image()
    create_lotsoffiles_image()
    create_utf8_fnames_image()
    create_modtimes_image()


