//===--- raw_ostream.h - Raw output stream ----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the raw_ostream class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_RAW_OSTREAM_H
#define LLVM_SUPPORT_RAW_OSTREAM_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"

namespace llvm {
  class format_object_base;
  template <typename T>
  class SmallVectorImpl;

/// raw_ostream - This class implements an extremely fast bulk output stream
/// that can *only* output to a stream.  It does not support seeking, reopening,
/// rewinding, line buffered disciplines etc. It is a simple buffer that outputs
/// a chunk at a time.
class raw_ostream {
private:
  // Do not implement. raw_ostream is noncopyable.
  void operator=(const raw_ostream &);
  raw_ostream(const raw_ostream &);

  /// The buffer is handled in such a way that the buffer is
  /// uninitialized, unbuffered, or out of space when OutBufCur >=
  /// OutBufEnd. Thus a single comparison suffices to determine if we
  /// need to take the slow path to write a single character.
  ///
  /// The buffer is in one of three states:
  ///  1. Unbuffered (BufferMode == Unbuffered)
  ///  1. Uninitialized (BufferMode != Unbuffered && OutBufStart == 0).
  ///  2. Buffered (BufferMode != Unbuffered && OutBufStart != 0 &&
  ///               OutBufEnd - OutBufStart >= 1).
  ///
  /// If buffered, then the raw_ostream owns the buffer if (BufferMode ==
  /// InternalBuffer); otherwise the buffer has been set via SetBuffer and is
  /// managed by the subclass.
  ///
  /// If a subclass installs an external buffer using SetBuffer then it can wait
  /// for a \see write_impl() call to handle the data which has been put into
  /// this buffer.
  char *OutBufStart, *OutBufEnd, *OutBufCur;
  
  enum BufferKind {
    Unbuffered = 0,
    InternalBuffer,
    ExternalBuffer
  } BufferMode;

  /// Error This flag is true if an error of any kind has been detected.
  ///
  bool Error;

public:
  // color order matches ANSI escape sequence, don't change
  enum Colors {
    BLACK=0,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    WHITE,
    SAVEDCOLOR
  };

  explicit raw_ostream(bool unbuffered=false)
    : BufferMode(unbuffered ? Unbuffered : InternalBuffer), Error(false) {
    // Start out ready to flush.
    OutBufStart = OutBufEnd = OutBufCur = 0;
  }

  virtual ~raw_ostream();

  /// tell - Return the current offset with the file.
  uint64_t tell() { return current_pos() + GetNumBytesInBuffer(); }

  /// has_error - Return the value of the flag in this raw_ostream indicating
  /// whether an output error has been encountered.
  bool has_error() const {
    return Error;
  }

  /// clear_error - Set the flag read by has_error() to false. If the error
  /// flag is set at the time when this raw_ostream's destructor is called,
  /// llvm_report_error is called to report the error. Use clear_error()
  /// after handling the error to avoid this behavior.
  void clear_error() {
    Error = false;
  }

  //===--------------------------------------------------------------------===//
  // Configuration Interface
  //===--------------------------------------------------------------------===//

  /// SetBuffered - Set the stream to be buffered, with an automatically
  /// determined buffer size.
  void SetBuffered();

  /// SetBufferSize - Set the stream to be buffered, using the
  /// specified buffer size.
  void SetBufferSize(size_t Size) {
    flush();
    SetBufferAndMode(new char[Size], Size, InternalBuffer);
  }

  size_t GetBufferSize() {
    // If we're supposed to be buffered but haven't actually gotten around
    // to allocating the buffer yet, return the value that would be used.
    if (BufferMode != Unbuffered && OutBufStart == 0)
      return preferred_buffer_size();

    // Otherwise just return the size of the allocated buffer.
    return OutBufEnd - OutBufStart;
  }

  /// SetUnbuffered - Set the stream to be unbuffered. When
  /// unbuffered, the stream will flush after every write. This routine
  /// will also flush the buffer immediately when the stream is being
  /// set to unbuffered.
  void SetUnbuffered() {
    flush();
    SetBufferAndMode(0, 0, Unbuffered);
  }

  size_t GetNumBytesInBuffer() const {
    return OutBufCur - OutBufStart;
  }

  //===--------------------------------------------------------------------===//
  // Data Output Interface
  //===--------------------------------------------------------------------===//

  void flush() {
    if (OutBufCur != OutBufStart)
      flush_nonempty();
  }

  raw_ostream &operator<<(char C) {
    if (OutBufCur >= OutBufEnd)
      return write(C);
    *OutBufCur++ = C;
    return *this;
  }

  raw_ostream &operator<<(unsigned char C) {
    if (OutBufCur >= OutBufEnd)
      return write(C);
    *OutBufCur++ = C;
    return *this;
  }

  raw_ostream &operator<<(signed char C) {
    if (OutBufCur >= OutBufEnd)
      return write(C);
    *OutBufCur++ = C;
    return *this;
  }

  raw_ostream &operator<<(const StringRef &Str) {
    // Inline fast path, particularly for strings with a known length.
    size_t Size = Str.size();

    // Make sure we can use the fast path.
    if (OutBufCur+Size > OutBufEnd)
      return write(Str.data(), Size);

    memcpy(OutBufCur, Str.data(), Size);
    OutBufCur += Size;
    return *this;
  }

  raw_ostream &operator<<(const char *Str) {
    // Inline fast path, particulary for constant strings where a sufficiently
    // smart compiler will simplify strlen.

    this->operator<<(StringRef(Str));
    return *this;
  }

  raw_ostream &operator<<(const std::string &Str) {
    // Avoid the fast path, it would only increase code size for a marginal win.
    write(Str.data(), Str.length());
    return *this;
  }

  raw_ostream &operator<<(unsigned long N);
  raw_ostream &operator<<(long N);
  raw_ostream &operator<<(unsigned long long N);
  raw_ostream &operator<<(long long N);
  raw_ostream &operator<<(const void *P);
  raw_ostream &operator<<(unsigned int N) {
    this->operator<<(static_cast<unsigned long>(N));
    return *this;
  }

  raw_ostream &operator<<(int N) {
    this->operator<<(static_cast<long>(N));
    return *this;
  }

  raw_ostream &operator<<(double N);  

  /// write_hex - Output \arg N in hexadecimal, without any prefix or padding.
  raw_ostream &write_hex(unsigned long long N);

  raw_ostream &write(unsigned char C);
  raw_ostream &write(const char *Ptr, size_t Size);

  // Formatted output, see the format() function in Support/Format.h.
  raw_ostream &operator<<(const format_object_base &Fmt);

  /// indent - Insert 'NumSpaces' spaces.
  raw_ostream &indent(unsigned NumSpaces);
  
  
  /// Changes the foreground color of text that will be output from this point
  /// forward.
  /// @param colors ANSI color to use, the special SAVEDCOLOR can be used to
  /// change only the bold attribute, and keep colors untouched
  /// @param bold bold/brighter text, default false
  /// @param bg if true change the background, default: change foreground
  /// @returns itself so it can be used within << invocations
  virtual raw_ostream &changeColor(enum Colors, bool, bool) { return *this; }

  /// Resets the colors to terminal defaults. Call this when you are done
  /// outputting colored text, or before program exit.
  virtual raw_ostream &resetColor() { return *this; }

  /// This function determines if this stream is connected to a "tty" or
  /// "console" window. That is, the output would be displayed to the user
  /// rather than being put on a pipe or stored in a file.
  virtual bool is_displayed() const { return false; }

  //===--------------------------------------------------------------------===//
  // Subclass Interface
  //===--------------------------------------------------------------------===//

private:
  /// write_impl - The is the piece of the class that is implemented
  /// by subclasses.  This writes the \args Size bytes starting at
  /// \arg Ptr to the underlying stream.
  /// 
  /// This function is guaranteed to only be called at a point at which it is
  /// safe for the subclass to install a new buffer via SetBuffer.
  ///
  /// \arg Ptr - The start of the data to be written. For buffered streams this
  /// is guaranteed to be the start of the buffer.
  /// \arg Size - The number of bytes to be written.
  ///
  /// \invariant { Size > 0 }
  virtual void write_impl(const char *Ptr, size_t Size) = 0;

  // An out of line virtual method to provide a home for the class vtable.
  virtual void handle();

  /// current_pos - Return the current position within the stream, not
  /// counting the bytes currently in the buffer.
  virtual uint64_t current_pos() = 0;

protected:
  /// SetBuffer - Use the provided buffer as the raw_ostream buffer. This is
  /// intended for use only by subclasses which can arrange for the output to go
  /// directly into the desired output buffer, instead of being copied on each
  /// flush.
  void SetBuffer(char *BufferStart, size_t Size) {
    SetBufferAndMode(BufferStart, Size, ExternalBuffer);
  }

  /// preferred_buffer_size - Return an efficient buffer size for the
  /// underlying output mechanism.
  virtual size_t preferred_buffer_size();

  /// error_detected - Set the flag indicating that an output error has
  /// been encountered.
  void error_detected() { Error = true; }

  /// getBufferStart - Return the beginning of the current stream buffer, or 0
  /// if the stream is unbuffered.
  const char *getBufferStart() const { return OutBufStart; }

  //===--------------------------------------------------------------------===//
  // Private Interface
  //===--------------------------------------------------------------------===//
private:
  /// SetBufferAndMode - Install the given buffer and mode.
  void SetBufferAndMode(char *BufferStart, size_t Size, BufferKind Mode);

  /// flush_nonempty - Flush the current buffer, which is known to be
  /// non-empty. This outputs the currently buffered data and resets
  /// the buffer to empty.
  void flush_nonempty();

  /// copy_to_buffer - Copy data into the buffer. Size must not be
  /// greater than the number of unused bytes in the buffer.
  void copy_to_buffer(const char *Ptr, size_t Size);
};

//===----------------------------------------------------------------------===//
// File Output Streams
//===----------------------------------------------------------------------===//

/// raw_fd_ostream - A raw_ostream that writes to a file descriptor.
///
class raw_fd_ostream : public raw_ostream {
  int FD;
  bool ShouldClose;
  uint64_t pos;

  /// write_impl - See raw_ostream::write_impl.
  virtual void write_impl(const char *Ptr, size_t Size);

  /// current_pos - Return the current position within the stream, not
  /// counting the bytes currently in the buffer.
  virtual uint64_t current_pos() { return pos; }

  /// preferred_buffer_size - Determine an efficient buffer size.
  virtual size_t preferred_buffer_size();

public:
  
  enum {
    /// F_Excl - When opening a file, this flag makes raw_fd_ostream
    /// report an error if the file already exists.
    F_Excl  = 1,

    /// F_Append - When opening a file, if it already exists append to the
    /// existing file instead of returning an error.  This may not be specified
    /// with F_Excl.
    F_Append = 2,

    /// F_Binary - The file should be opened in binary mode on platforms that
    /// make this distinction.
    F_Binary = 4
  };
  
  /// raw_fd_ostream - Open the specified file for writing. If an error occurs,
  /// information about the error is put into ErrorInfo, and the stream should
  /// be immediately destroyed; the string will be empty if no error occurred.
  /// This allows optional flags to control how the file will be opened.
  ///
  /// \param Filename - The file to open. If this is "-" then the
  /// stream will use stdout instead.
  raw_fd_ostream(const char *Filename, std::string &ErrorInfo,
                 unsigned Flags = 0);

  /// raw_fd_ostream ctor - FD is the file descriptor that this writes to.  If
  /// ShouldClose is true, this closes the file when the stream is destroyed.
  raw_fd_ostream(int fd, bool shouldClose, 
                 bool unbuffered=false) : raw_ostream(unbuffered), FD(fd), 
                                          ShouldClose(shouldClose) {}
  
  ~raw_fd_ostream();

  /// close - Manually flush the stream and close the file.
  void close();

  /// seek - Flushes the stream and repositions the underlying file descriptor
  ///  positition to the offset specified from the beginning of the file.
  uint64_t seek(uint64_t off);

  virtual raw_ostream &changeColor(enum Colors colors, bool bold=false,
                                   bool bg=false);
  virtual raw_ostream &resetColor();

  virtual bool is_displayed() const;
};

/// raw_stdout_ostream - This is a stream that always prints to stdout.
///
class raw_stdout_ostream : public raw_fd_ostream {
  // An out of line virtual method to provide a home for the class vtable.
  virtual void handle();
public:
  raw_stdout_ostream();
};

/// raw_stderr_ostream - This is a stream that always prints to stderr.
///
class raw_stderr_ostream : public raw_fd_ostream {
  // An out of line virtual method to provide a home for the class vtable.
  virtual void handle();
public:
  raw_stderr_ostream();
};

/// outs() - This returns a reference to a raw_ostream for standard output.
/// Use it like: outs() << "foo" << "bar";
raw_ostream &outs();

/// errs() - This returns a reference to a raw_ostream for standard error.
/// Use it like: errs() << "foo" << "bar";
raw_ostream &errs();

/// nulls() - This returns a reference to a raw_ostream which simply discards
/// output.
raw_ostream &nulls();

//===----------------------------------------------------------------------===//
// Output Stream Adaptors
//===----------------------------------------------------------------------===//

/// raw_string_ostream - A raw_ostream that writes to an std::string.  This is a
/// simple adaptor class. This class does not encounter output errors.
class raw_string_ostream : public raw_ostream {
  std::string &OS;

  /// write_impl - See raw_ostream::write_impl.
  virtual void write_impl(const char *Ptr, size_t Size);

  /// current_pos - Return the current position within the stream, not
  /// counting the bytes currently in the buffer.
  virtual uint64_t current_pos() { return OS.size(); }
public:
  explicit raw_string_ostream(std::string &O) : OS(O) {}
  ~raw_string_ostream();

  /// str - Flushes the stream contents to the target string and returns
  ///  the string's reference.
  std::string& str() {
    flush();
    return OS;
  }
};

/// raw_svector_ostream - A raw_ostream that writes to an SmallVector or
/// SmallString.  This is a simple adaptor class. This class does not
/// encounter output errors.
class raw_svector_ostream : public raw_ostream {
  SmallVectorImpl<char> &OS;

  /// write_impl - See raw_ostream::write_impl.
  virtual void write_impl(const char *Ptr, size_t Size);

  /// current_pos - Return the current position within the stream, not
  /// counting the bytes currently in the buffer.
  virtual uint64_t current_pos();
public:
  /// Construct a new raw_svector_ostream.
  ///
  /// \arg O - The vector to write to; this should generally have at least 128
  /// bytes free to avoid any extraneous memory overhead.
  explicit raw_svector_ostream(SmallVectorImpl<char> &O);
  ~raw_svector_ostream();

  /// str - Flushes the stream contents to the target vector and return a
  /// StringRef for the vector contents.
  StringRef str();
};

/// raw_null_ostream - A raw_ostream that discards all output.
class raw_null_ostream : public raw_ostream {
  /// write_impl - See raw_ostream::write_impl.
  virtual void write_impl(const char *Ptr, size_t size);
  
  /// current_pos - Return the current position within the stream, not
  /// counting the bytes currently in the buffer.
  virtual uint64_t current_pos();

public:
  explicit raw_null_ostream() {}
  ~raw_null_ostream();
};

} // end llvm namespace

#endif
