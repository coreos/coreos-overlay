// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "files/file_path.h"

#include <ostream>

#include "base/basictypes.h"
#include "base/logging.h"

namespace files {

using StringType = FilePath::StringType;

namespace {

const FilePath::CharType kStringTerminator = FILE_PATH_LITERAL('\0');

bool IsPathAbsolute(StringType path) {
  // Look for a separator in the first position.
  return path.length() > 0 && FilePath::IsSeparator(path[0]);
}

bool AreAllSeparators(const StringType& input) {
  for (StringType::const_iterator it = input.begin();
      it != input.end(); ++it) {
    if (!FilePath::IsSeparator(*it))
      return false;
  }

  return true;
}
}  // namespace

FilePath::FilePath() {
}

FilePath::FilePath(const FilePath& that) : path_(that.path_) {
}

FilePath::FilePath(const StringType& path) : path_(path) {
  StringType::size_type nul_pos = path_.find(kStringTerminator);
  if (nul_pos != StringType::npos)
    path_.erase(nul_pos, StringType::npos);
}

FilePath::~FilePath() {
}

FilePath& FilePath::operator=(const FilePath& that) {
  path_ = that.path_;
  return *this;
}

bool FilePath::operator==(const FilePath& that) const {
  return path_ == that.path_;
}

bool FilePath::operator!=(const FilePath& that) const {
  return path_ != that.path_;
}

// static
bool FilePath::IsSeparator(CharType character) {
  for (size_t i = 0; i < kSeparatorsLength - 1; ++i) {
    if (character == kSeparators[i]) {
      return true;
    }
  }

  return false;
}

void FilePath::GetComponents(std::vector<StringType>* components) const {
  DCHECK(components);
  if (!components)
    return;
  components->clear();
  if (value().empty())
    return;

  std::vector<StringType> ret_val;
  FilePath current = *this;
  FilePath base;

  // Capture path components.
  while (current != current.DirName()) {
    base = current.BaseName();
    if (!AreAllSeparators(base.value()))
      ret_val.push_back(base.value());
    current = current.DirName();
  }

  // Capture root, if any.
  base = current.BaseName();
  if (!base.value().empty() && base.value() != kCurrentDirectory)
    ret_val.push_back(current.BaseName().value());

  *components = std::vector<StringType>(ret_val.rbegin(), ret_val.rend());
}

bool FilePath::IsParent(const FilePath& child) const {
  return AppendRelativePath(child, NULL);
}

bool FilePath::AppendRelativePath(const FilePath& child,
                                  FilePath* path) const {
  std::vector<StringType> parent_components;
  std::vector<StringType> child_components;
  GetComponents(&parent_components);
  child.GetComponents(&child_components);

  if (parent_components.empty() ||
      parent_components.size() >= child_components.size())
    return false;

  std::vector<StringType>::const_iterator parent_comp =
      parent_components.begin();
  std::vector<StringType>::const_iterator child_comp =
      child_components.begin();

  while (parent_comp != parent_components.end()) {
    if (*parent_comp != *child_comp)
      return false;
    ++parent_comp;
    ++child_comp;
  }

  if (path != NULL) {
    for (; child_comp != child_components.end(); ++child_comp) {
      *path = path->Append(*child_comp);
    }
  }
  return true;
}

// libgen's dirname and basename aren't guaranteed to be thread-safe and aren't
// guaranteed to not modify their input strings, and in fact are implemented
// differently in this regard on different platforms.  Don't use them, but
// adhere to their behavior.
FilePath FilePath::DirName() const {
  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  StringType::size_type last_separator =
      new_path.path_.find_last_of(kSeparators, StringType::npos,
                                  kSeparatorsLength - 1);
  if (last_separator == StringType::npos) {
    // path_ is in the current directory.
    new_path.path_.resize(0);
  } else if (last_separator == 0) {
    // path_ is in the root directory.
    new_path.path_.resize(1);
  } else if (last_separator == 1 && IsSeparator(new_path.path_[0])) {
    // path_ is in "//"; intact indicating alternate root.
    new_path.path_.resize(2);
  } else if (last_separator != 0) {
    // path_ is somewhere else, trim the basename.
    new_path.path_.resize(last_separator);
  }

  new_path.StripTrailingSeparatorsInternal();
  if (!new_path.path_.length())
    new_path.path_ = kCurrentDirectory;

  return new_path;
}

FilePath FilePath::BaseName() const {
  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  // Keep everything after the final separator, but if the pathname is only
  // one character and it's a separator, leave it alone.
  StringType::size_type last_separator =
      new_path.path_.find_last_of(kSeparators, StringType::npos,
                                  kSeparatorsLength - 1);
  if (last_separator != StringType::npos &&
      last_separator < new_path.path_.length() - 1) {
    new_path.path_.erase(0, last_separator + 1);
  }

  return new_path;
}

FilePath FilePath::Append(StringType component) const {
  DCHECK(!IsPathAbsolute(component));

  StringType::size_type nul_pos = component.find(kStringTerminator);
  if (nul_pos != StringType::npos)
    component.resize(nul_pos);

  if (path_.compare(kCurrentDirectory) == 0) {
    // Append normally doesn't do any normalization, but as a special case,
    // when appending to kCurrentDirectory, just return a new path for the
    // component argument.  Appending component to kCurrentDirectory would
    // serve no purpose other than needlessly lengthening the path, and
    // it's likely in practice to wind up with FilePath objects containing
    // only kCurrentDirectory when calling DirName on a single relative path
    // component.
    return FilePath(component);
  }

  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  // Don't append a separator if the path is empty (indicating the current
  // directory) or if the path component is empty (indicating nothing to
  // append).
  if (component.length() > 0 && new_path.path_.length() > 0) {
    // Don't append a separator if the path still ends with a trailing
    // separator after stripping (indicating the root directory).
    if (!IsSeparator(new_path.path_[new_path.path_.length() - 1])) {
      new_path.path_.append(1, kSeparators[0]);
    }
  }

  new_path.path_ += component;
  return new_path;
}

FilePath FilePath::Append(const FilePath& component) const {
  return Append(component.value());
}

bool FilePath::IsAbsolute() const {
  return IsPathAbsolute(path_);
}

bool FilePath::EndsWithSeparator() const {
  if (empty())
    return false;
  return IsSeparator(path_[path_.size() - 1]);
}

FilePath FilePath::AsEndingWithSeparator() const {
  if (EndsWithSeparator() || path_.empty())
    return *this;

  StringType path_str;
  path_str.reserve(path_.length() + 1);  // Only allocate string once.

  path_str = path_;
  path_str.append(&kSeparators[0], 1);
  return FilePath(path_str);
}

FilePath FilePath::StripTrailingSeparators() const {
  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  return new_path;
}

bool FilePath::ReferencesParent() const {
  std::vector<StringType> components;
  GetComponents(&components);

  std::vector<StringType>::const_iterator it = components.begin();
  for (; it != components.end(); ++it) {
    const StringType& component = *it;
    // Windows has odd, undocumented behavior with path components containing
    // only whitespace and . characters. So, if all we see is . and
    // whitespace, then we treat any .. sequence as referencing parent.
    // For simplicity we enforce this on all platforms.
    if (component.find_first_not_of(FILE_PATH_LITERAL(". \n\r\t")) ==
            std::string::npos &&
        component.find(kParentDirectory) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void FilePath::StripTrailingSeparatorsInternal() {
  StringType::size_type last_stripped = StringType::npos;
  // Prevent stripping the leading separator if there is only one separator.
  for (StringType::size_type pos = path_.length();
       pos > 1 && IsSeparator(path_[pos - 1]);
       --pos) {
    // If the string only has two separators and they're at the beginning,
    // don't strip them, unless the string began with more than two separators.
    if (pos != 2 || last_stripped == 3 || !IsSeparator(path_[0])) {
      path_.resize(pos - 1);
      last_stripped = pos;
    }
  }
}

void PrintTo(const FilePath& path, std::ostream* out) {
  *out << path.value();
}

}  // namespace files
