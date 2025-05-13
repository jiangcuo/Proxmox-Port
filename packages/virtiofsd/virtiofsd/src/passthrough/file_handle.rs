// Copyright 2021 Red Hat, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::oslib;
use crate::passthrough::mount_fd::{MPRResult, MountFd, MountFds};
use crate::passthrough::stat::MountId;
use std::ffi::CStr;
use std::fs::File;
use std::io;
use std::os::unix::io::{AsRawFd, RawFd};
use std::sync::Arc;

const EMPTY_CSTR: &[u8] = b"\0";

#[derive(Clone, PartialOrd, Ord, PartialEq, Eq)]
pub struct FileHandle {
    mnt_id: MountId,
    handle: oslib::CFileHandle,
}

pub struct OpenableFileHandle {
    handle: FileHandle,
    mount_fd: Arc<MountFd>,
}

pub enum FileOrHandle {
    File(File),
    Handle(OpenableFileHandle),
}

impl FileHandle {
    /// Create a file handle for the given file.
    ///
    /// Return `Ok(None)` if no file handle can be generated for this file: Either because the
    /// filesystem does not support it, or because it would require a larger file handle than we
    /// can store.  These are not intermittent failures, i.e. if this function returns `Ok(None)`
    /// for a specific file, it will always return `Ok(None)` for it.  Conversely, if this function
    /// returns `Ok(Some)` at some point, it will never return `Ok(None)` later.
    ///
    /// Return an `io::Error` for all other errors.
    pub fn from_name_at(dir: &impl AsRawFd, path: &CStr) -> io::Result<Option<Self>> {
        let mut mount_id: libc::c_int = 0;
        let mut c_fh = oslib::CFileHandle::default();

        if let Err(err) =
            oslib::name_to_handle_at(dir, path, &mut c_fh, &mut mount_id, libc::AT_EMPTY_PATH)
        {
            match err.raw_os_error() {
                // Filesystem does not support file handles
                Some(libc::EOPNOTSUPP) => Ok(None),
                // Handle would need more bytes than `MAX_HANDLE_SZ`
                Some(libc::EOVERFLOW) => Ok(None),
                // Other error
                _ => Err(err),
            }
        } else {
            Ok(Some(FileHandle {
                mnt_id: mount_id as MountId,
                handle: c_fh,
            }))
        }
    }

    /// Create a file handle for `fd`.
    /// This is a wrapper around `from_name_at()` and so has the same interface.
    pub fn from_fd(fd: &impl AsRawFd) -> io::Result<Option<Self>> {
        // Safe because this is a constant value and a valid C string.
        let empty_path = unsafe { CStr::from_bytes_with_nul_unchecked(EMPTY_CSTR) };
        Self::from_name_at(fd, empty_path)
    }

    /**
     * Return an openable copy of the file handle by ensuring that `mount_fds` contains a valid fd
     * for the mount the file handle is for.
     *
     * `reopen_fd` will be invoked to duplicate an `O_PATH` fd with custom `libc::open()` flags.
     */
    pub fn to_openable<F>(
        &self,
        mount_fds: &MountFds,
        reopen_fd: F,
    ) -> MPRResult<OpenableFileHandle>
    where
        F: FnOnce(RawFd, libc::c_int) -> io::Result<File>,
    {
        Ok(OpenableFileHandle {
            handle: self.clone(),
            mount_fd: mount_fds.get(self.mnt_id, reopen_fd)?,
        })
    }
}

impl OpenableFileHandle {
    pub fn inner(&self) -> &FileHandle {
        &self.handle
    }

    /**
     * Open a file handle, using our mount FDs hash map.
     */
    pub fn open(&self, flags: libc::c_int) -> io::Result<File> {
        oslib::open_by_handle_at(self.mount_fd.file(), &self.handle.handle, flags)
    }
}
