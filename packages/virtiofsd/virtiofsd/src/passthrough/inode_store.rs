// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE-BSD-3-Clause file.

use crate::passthrough::file_handle::{FileHandle, FileOrHandle};
use crate::passthrough::stat::MountId;
use crate::passthrough::util::{ebadf, is_safe_inode, reopen_fd_through_proc};
use std::collections::BTreeMap;
use std::fs::File;
use std::io;
use std::os::unix::io::{AsRawFd, RawFd};
use std::sync::atomic::AtomicU64;
use std::sync::Arc;

pub type Inode = u64;

#[derive(Clone, Copy, Eq, Ord, PartialEq, PartialOrd)]
pub struct InodeIds {
    pub ino: libc::ino64_t,
    pub dev: libc::dev_t,
    pub mnt_id: MountId,
}

pub struct InodeData {
    pub inode: Inode,
    // Most of these aren't actually files but ¯\_(ツ)_/¯.
    pub file_or_handle: FileOrHandle,
    pub refcount: AtomicU64,

    // Used as key in the `InodeStore::by_ids` map.
    pub ids: InodeIds,

    // File type and mode
    pub mode: u32,
}

/**
 * Represents the file associated with an inode (`InodeData`).
 *
 * When obtaining such a file, it may either be a new file (the `Owned` variant), in which case the
 * object's lifetime is static, or it may reference `InodeData.file` (the `Ref` variant), in which
 * case the object's lifetime is that of the respective `InodeData` object.
 */
pub enum InodeFile<'inode_lifetime> {
    Owned(File),
    Ref(&'inode_lifetime File),
}

#[derive(Default)]
pub struct InodeStore {
    data: BTreeMap<Inode, Arc<InodeData>>,
    by_ids: BTreeMap<InodeIds, Inode>,
    by_handle: BTreeMap<FileHandle, Inode>,
}

impl<'a> InodeData {
    /// Get an `O_PATH` file for this inode
    pub fn get_file(&'a self) -> io::Result<InodeFile<'a>> {
        match &self.file_or_handle {
            FileOrHandle::File(f) => Ok(InodeFile::Ref(f)),
            FileOrHandle::Handle(h) => {
                let file = h.open(libc::O_PATH)?;
                Ok(InodeFile::Owned(file))
            }
        }
    }

    /// Open this inode with the given flags
    /// (always returns a new (i.e. `Owned`) file, hence the static lifetime)
    pub fn open_file(
        &self,
        flags: libc::c_int,
        proc_self_fd: &File,
    ) -> io::Result<InodeFile<'static>> {
        if !is_safe_inode(self.mode) {
            return Err(ebadf());
        }

        match &self.file_or_handle {
            FileOrHandle::File(f) => {
                let new_file = reopen_fd_through_proc(f, flags, proc_self_fd)?;
                Ok(InodeFile::Owned(new_file))
            }
            FileOrHandle::Handle(h) => {
                let new_file = h.open(flags)?;
                Ok(InodeFile::Owned(new_file))
            }
        }
    }
}

impl InodeFile<'_> {
    /// Create a standalone `File` object
    pub fn into_file(self) -> io::Result<File> {
        match self {
            Self::Owned(file) => Ok(file),
            Self::Ref(file_ref) => file_ref.try_clone(),
        }
    }
}

impl AsRawFd for InodeFile<'_> {
    /// Return a file descriptor for this file
    /// Note: This fd is only valid as long as the `InodeFile` exists.
    fn as_raw_fd(&self) -> RawFd {
        match self {
            Self::Owned(file) => file.as_raw_fd(),
            Self::Ref(file_ref) => file_ref.as_raw_fd(),
        }
    }
}

impl InodeStore {
    pub fn insert(&mut self, data: Arc<InodeData>) {
        self.by_ids.insert(data.ids, data.inode);
        if let FileOrHandle::Handle(handle) = &data.file_or_handle {
            self.by_handle.insert(handle.inner().clone(), data.inode);
        }
        self.data.insert(data.inode, data);
    }

    pub fn remove(&mut self, inode: &Inode) -> Option<Arc<InodeData>> {
        let data = self.data.remove(inode);
        if let Some(data) = data.as_ref() {
            if let FileOrHandle::Handle(handle) = &data.file_or_handle {
                self.by_handle.remove(handle.inner());
            }
            self.by_ids.remove(&data.ids);
        }
        data
    }

    pub fn clear(&mut self) {
        self.data.clear();
        self.by_handle.clear();
        self.by_ids.clear();
    }

    pub fn get(&self, inode: &Inode) -> Option<&Arc<InodeData>> {
        self.data.get(inode)
    }

    pub fn get_by_ids(&self, ids: &InodeIds) -> Option<&Arc<InodeData>> {
        self.inode_by_ids(ids).map(|inode| self.get(inode).unwrap())
    }

    pub fn get_by_handle(&self, handle: &FileHandle) -> Option<&Arc<InodeData>> {
        self.inode_by_handle(handle)
            .map(|inode| self.get(inode).unwrap())
    }

    pub fn inode_by_ids(&self, ids: &InodeIds) -> Option<&Inode> {
        self.by_ids.get(ids)
    }

    pub fn inode_by_handle(&self, handle: &FileHandle) -> Option<&Inode> {
        self.by_handle.get(handle)
    }
}
