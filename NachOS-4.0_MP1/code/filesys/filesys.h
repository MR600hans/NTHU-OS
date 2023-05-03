// filesys.h
//	Data structures to represent the Nachos file system.
//
//	A file system is a set of files stored on disk, organized
//	into directories.  Operations on the file system have to
//	do with "naming" -- creating, opening, and deleting files,
//	given a textual file name.  Operations on an individual
//	"open" file (read, write, close) are to be found in the OpenFile
//	class (openfile.h).
//
//	We define two separate implementations of the file system.
//	The "STUB" version just re-defines the Nachos file system
//	operations as operations on the native UNIX file system on the machine
//	running the Nachos simulation.
//
//	The other version is a "real" file system, built on top of
//	a disk simulator.  The disk is simulated using the native UNIX
//	file system (in a file named "DISK").
//
//	In the "real" implementation, there are two key data structures used
//	in the file system.  There is a single "root" directory, listing
//	all of the files in the file system; unlike UNIX, the baseline
//	system does not provide a hierarchical directory structure.
//	In addition, there is a bitmap for allocating
//	disk sectors.  Both the root directory and the bitmap are themselves
//	stored as files in the Nachos file system -- this causes an interesting
//	bootstrap problem when the simulated disk is initialized.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef FS_H
#define FS_H

#include "copyright.h"
#include "debug.h"  //just for test!!!
#include "openfile.h"
#include "sysdep.h"

#ifdef FILESYS_STUB  // Temporarily implement file system calls as
                     // calls to UNIX, until the real file system
                     // implementation is available
typedef int OpenFileId;

class FileSystem {
 public:
  FileSystem() {
    for (int i = 0; i < 20; i++) OpenFileTable[i] = NULL;
  }

  bool Create(char *name) {
    int fileDescriptor = OpenForWrite(name);

    if (fileDescriptor == -1) return FALSE;
    Close(fileDescriptor);
    return TRUE;
  }
  // The OpenFile function is used for open user program  [userprog/addrspace.cc]
  OpenFile *Open(char *name) {
    int fileDescriptor = OpenForReadWrite(name, FALSE);
    if (fileDescriptor == -1) return NULL;
    return new OpenFile(fileDescriptor);
  }

  // The OpenAFile function is used for kernel open system call
  /* TODO (Open)
    1) If the file is not exist or OpenFileTable is full, return -1
    2) Otherwise, find the empty table to place the new created OpenFile and return its index.
  */
  OpenFileId OpenAFile(char *name) {
    int fileDescriptor = OpenForReadWrite(name, FALSE);
    if (fileDescriptor == -1)
      return -1;
    OpenFile *newFile = new OpenFile(fileDescriptor);

    for (int i = 0; i < 20; i++)
    {
      if (OpenFileTable[i] == NULL)
      {
        OpenFileTable[i] = newFile;
        return i + 1;
      }
    }
    return -1;
  }

  // The WriteFile function is used for kernel write system call
  /* TODO (Write)
    1) If the id is out of range or indicates to a non-exist file, return -1
    2) Otherwise, call OpenFile function to execute write and return the number of characters. 
  */
  int WriteAFile(char *buffer, int size, OpenFileId id) {
    OpenFile *OpenFilePT = OpenFileTable[id - 1];
    if (OpenFilePT == NULL)
      return -1;

    int numWritten = OpenFilePT->Write(buffer, size);
    return numWritten;
  }

  // The ReadFile function is used for kernel read system call
  /* TODO (Read)
    1) If the id is out of range or indicates to a non-exist file, return -1
    2) Otherwise, call OpenFile function to execute read and return the number of characters. 
  */
  int ReadFile(char *buffer, int size, OpenFileId id) {
    OpenFile *OpenFilePT = OpenFileTable[id - 1];
    if (OpenFilePT == NULL)
      return -1;

    int numRead = OpenFilePT->Read(buffer, size);
    return numRead;
  }

  // The CloseFile function is used for kernel close system call
  /* TODO (Close)
    1) If the id is out of range or indicates to a non-exist file, return -1
    2) Otherwise, delete the open file and clear its open file table.
  */ 
  int CloseFile(OpenFileId id) {
    OpenFile *OpenFilePT = OpenFileTable[id - 1];
    if (OpenFilePT == NULL)
      return -1;

    OpenFileTable[id - 1] = NULL;
    return 1;
  }

  bool Remove(char *name) { return Unlink(name) == 0; }

  OpenFile *OpenFileTable[20];
};

#else  // FILESYS
class FileSystem {
 public:
  FileSystem(bool format);  // Initialize the file system.
                            // Must be called *after* "synchDisk"
                            // has been initialized.
                            // If "format", there is nothing on
                            // the disk, so initialize the directory
                            // and the bitmap of free blocks.

  bool Create(char* name, int initialSize);
  // Create a file (UNIX creat)

  OpenFile* Open(char* name);  // Open a file (UNIX open)

  bool Remove(char* name);  // Delete a file (UNIX unlink)

  void List();  // List all the files in the file system

  void Print();  // List all the files and their contents

 private:
  OpenFile* freeMapFile;  // Bit map of free disk blocks,
                          // represented as a file
  OpenFile* directoryFile;  // "Root" directory -- list of
                            // file names, represented as a file
};

#endif  // FILESYS

#endif  // FS_H
