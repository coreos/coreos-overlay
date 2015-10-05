// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include <gtest/gtest.h>

#include "files/file_path.h"
#include "macros.h"

// This macro helps avoid wrapped lines in the test structs.
#define FPL(x) FILE_PATH_LITERAL(x)

// This macro constructs strings which can contain NULs.
#define FPS(x) FilePath::StringType(FPL(x), arraysize(FPL(x)) - 1)

namespace files {

struct UnaryTestData {
  const FilePath::CharType* input;
  const FilePath::CharType* expected;
};

struct UnaryBooleanTestData {
  const FilePath::CharType* input;
  bool expected;
};

struct BinaryTestData {
  const FilePath::CharType* inputs[2];
  const FilePath::CharType* expected;
};

struct BinaryBooleanTestData {
  const FilePath::CharType* inputs[2];
  bool expected;
};

struct BinaryIntTestData {
  const FilePath::CharType* inputs[2];
  int expected;
};

struct UTF8TestData {
  const FilePath::CharType* native;
  const char* utf8;
};

typedef testing::Test FilePathTest;

TEST_F(FilePathTest, DirName) {
  const struct UnaryTestData cases[] = {
    { FPL(""),              FPL(".") },
    { FPL("aa"),            FPL(".") },
    { FPL("/aa/bb"),        FPL("/aa") },
    { FPL("/aa/bb/"),       FPL("/aa") },
    { FPL("/aa/bb//"),      FPL("/aa") },
    { FPL("/aa/bb/ccc"),    FPL("/aa/bb") },
    { FPL("/aa"),           FPL("/") },
    { FPL("/aa/"),          FPL("/") },
    { FPL("/"),             FPL("/") },
    { FPL("//"),            FPL("//") },
    { FPL("///"),           FPL("/") },
    { FPL("aa/"),           FPL(".") },
    { FPL("aa/bb"),         FPL("aa") },
    { FPL("aa/bb/"),        FPL("aa") },
    { FPL("aa/bb//"),       FPL("aa") },
    { FPL("aa//bb//"),      FPL("aa") },
    { FPL("aa//bb/"),       FPL("aa") },
    { FPL("aa//bb"),        FPL("aa") },
    { FPL("//aa/bb"),       FPL("//aa") },
    { FPL("//aa/"),         FPL("//") },
    { FPL("//aa"),          FPL("//") },
    { FPL("0:"),            FPL(".") },
    { FPL("@:"),            FPL(".") },
    { FPL("[:"),            FPL(".") },
    { FPL("`:"),            FPL(".") },
    { FPL("{:"),            FPL(".") },
    { FPL("\xB3:"),         FPL(".") },
    { FPL("\xC5:"),         FPL(".") },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    FilePath observed = input.DirName();
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed.value()) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, BaseName) {
  const struct UnaryTestData cases[] = {
    { FPL(""),              FPL("") },
    { FPL("aa"),            FPL("aa") },
    { FPL("/aa/bb"),        FPL("bb") },
    { FPL("/aa/bb/"),       FPL("bb") },
    { FPL("/aa/bb//"),      FPL("bb") },
    { FPL("/aa/bb/ccc"),    FPL("ccc") },
    { FPL("/aa"),           FPL("aa") },
    { FPL("/"),             FPL("/") },
    { FPL("//"),            FPL("//") },
    { FPL("///"),           FPL("/") },
    { FPL("aa/"),           FPL("aa") },
    { FPL("aa/bb"),         FPL("bb") },
    { FPL("aa/bb/"),        FPL("bb") },
    { FPL("aa/bb//"),       FPL("bb") },
    { FPL("aa//bb//"),      FPL("bb") },
    { FPL("aa//bb/"),       FPL("bb") },
    { FPL("aa//bb"),        FPL("bb") },
    { FPL("//aa/bb"),       FPL("bb") },
    { FPL("//aa/"),         FPL("aa") },
    { FPL("//aa"),          FPL("aa") },
    { FPL("0:"),            FPL("0:") },
    { FPL("@:"),            FPL("@:") },
    { FPL("[:"),            FPL("[:") },
    { FPL("`:"),            FPL("`:") },
    { FPL("{:"),            FPL("{:") },
    { FPL("\xB3:"),         FPL("\xB3:") },
    { FPL("\xC5:"),         FPL("\xC5:") },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    FilePath observed = input.BaseName();
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed.value()) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, Append) {
  const struct BinaryTestData cases[] = {
    { { FPL(""),           FPL("cc") }, FPL("cc") },
    { { FPL("."),          FPL("ff") }, FPL("ff") },
    { { FPL("/"),          FPL("cc") }, FPL("/cc") },
    { { FPL("/aa"),        FPL("") },   FPL("/aa") },
    { { FPL("/aa/"),       FPL("") },   FPL("/aa") },
    { { FPL("//aa"),       FPL("") },   FPL("//aa") },
    { { FPL("//aa/"),      FPL("") },   FPL("//aa") },
    { { FPL("//"),         FPL("aa") }, FPL("//aa") },
    { { FPL("/aa/bb"),     FPL("cc") }, FPL("/aa/bb/cc") },
    { { FPL("/aa/bb/"),    FPL("cc") }, FPL("/aa/bb/cc") },
    { { FPL("aa/bb/"),     FPL("cc") }, FPL("aa/bb/cc") },
    { { FPL("aa/bb"),      FPL("cc") }, FPL("aa/bb/cc") },
    { { FPL("a/b"),        FPL("c") },  FPL("a/b/c") },
    { { FPL("a/b/"),       FPL("c") },  FPL("a/b/c") },
    { { FPL("//aa"),       FPL("bb") }, FPL("//aa/bb") },
    { { FPL("//aa/"),      FPL("bb") }, FPL("//aa/bb") },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath root(cases[i].inputs[0]);
    FilePath::StringType leaf(cases[i].inputs[1]);
    FilePath observed_str = root.Append(leaf);
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed_str.value()) <<
              "i: " << i << ", root: " << root.value() << ", leaf: " << leaf;
    FilePath observed_path = root.Append(FilePath(leaf));
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed_path.value()) <<
              "i: " << i << ", root: " << root.value() << ", leaf: " << leaf;
  }
}

TEST_F(FilePathTest, StripTrailingSeparators) {
  const struct UnaryTestData cases[] = {
    { FPL(""),              FPL("") },
    { FPL("/"),             FPL("/") },
    { FPL("//"),            FPL("//") },
    { FPL("///"),           FPL("/") },
    { FPL("////"),          FPL("/") },
    { FPL("a/"),            FPL("a") },
    { FPL("a//"),           FPL("a") },
    { FPL("a///"),          FPL("a") },
    { FPL("a////"),         FPL("a") },
    { FPL("/a"),            FPL("/a") },
    { FPL("/a/"),           FPL("/a") },
    { FPL("/a//"),          FPL("/a") },
    { FPL("/a///"),         FPL("/a") },
    { FPL("/a////"),        FPL("/a") },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    FilePath observed = input.StripTrailingSeparators();
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed.value()) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, IsAbsolute) {
  const struct UnaryBooleanTestData cases[] = {
    { FPL(""),       false },
    { FPL("a"),      false },
    { FPL("c:"),     false },
    { FPL("c:a"),    false },
    { FPL("a/b"),    false },
    { FPL("//"),     true },
    { FPL("//a"),    true },
    { FPL("c:a/b"),  false },
    { FPL("?:/a"),   false },
    { FPL("/"),      true },
    { FPL("/a"),     true },
    { FPL("/."),     true },
    { FPL("/.."),    true },
    { FPL("c:/"),    false },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    bool observed = input.IsAbsolute();
    EXPECT_EQ(cases[i].expected, observed) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, PathComponentsTest) {
  const struct UnaryTestData cases[] = {
    { FPL("//foo/bar/baz/"),          FPL("|//|foo|bar|baz")},
    { FPL("///"),                     FPL("|/")},
    { FPL("/foo//bar//baz/"),         FPL("|/|foo|bar|baz")},
    { FPL("/foo/bar/baz/"),           FPL("|/|foo|bar|baz")},
    { FPL("/foo/bar/baz//"),          FPL("|/|foo|bar|baz")},
    { FPL("/foo/bar/baz///"),         FPL("|/|foo|bar|baz")},
    { FPL("/foo/bar/baz"),            FPL("|/|foo|bar|baz")},
    { FPL("/foo/bar.bot/baz.txt"),    FPL("|/|foo|bar.bot|baz.txt")},
    { FPL("//foo//bar/baz"),          FPL("|//|foo|bar|baz")},
    { FPL("/"),                       FPL("|/")},
    { FPL("foo"),                     FPL("|foo")},
    { FPL(""),                        FPL("")},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    std::vector<FilePath::StringType> comps;
    input.GetComponents(&comps);

    FilePath::StringType observed;
    for (size_t j = 0; j < comps.size(); ++j) {
      observed.append(FILE_PATH_LITERAL("|"), 1);
      observed.append(comps[j]);
    }
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, IsParentTest) {
  const struct BinaryBooleanTestData cases[] = {
    { { FPL("/"),             FPL("/foo/bar/baz") },      true},
    { { FPL("/foo/bar"),      FPL("/foo/bar/baz") },      true},
    { { FPL("/foo/bar/"),     FPL("/foo/bar/baz") },      true},
    { { FPL("//foo/bar/"),    FPL("//foo/bar/baz") },     true},
    { { FPL("/foo/bar"),      FPL("/foo2/bar/baz") },     false},
    { { FPL("/foo/bar.txt"),  FPL("/foo/bar/baz") },      false},
    { { FPL("/foo/bar"),      FPL("/foo/bar2/baz") },     false},
    { { FPL("/foo/bar"),      FPL("/foo/bar") },          false},
    { { FPL("/foo/bar/baz"),  FPL("/foo/bar") },          false},
    { { FPL("foo/bar"),       FPL("foo/bar/baz") },       true},
    { { FPL("foo/bar"),       FPL("foo2/bar/baz") },      false},
    { { FPL("foo/bar"),       FPL("foo/bar2/baz") },      false},
    { { FPL(""),              FPL("foo") },               false},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath parent(cases[i].inputs[0]);
    FilePath child(cases[i].inputs[1]);

    EXPECT_EQ(parent.IsParent(child), cases[i].expected) <<
        "i: " << i << ", parent: " << parent.value() << ", child: " <<
        child.value();
  }
}

TEST_F(FilePathTest, AppendRelativePathTest) {
  const struct BinaryTestData cases[] = {
    { { FPL("/"),             FPL("/foo/bar/baz") },      FPL("foo/bar/baz")},
    { { FPL("/foo/bar"),      FPL("/foo/bar/baz") },      FPL("baz")},
    { { FPL("/foo/bar/"),     FPL("/foo/bar/baz") },      FPL("baz")},
    { { FPL("//foo/bar/"),    FPL("//foo/bar/baz") },     FPL("baz")},
    { { FPL("/foo/bar"),      FPL("/foo2/bar/baz") },     FPL("")},
    { { FPL("/foo/bar.txt"),  FPL("/foo/bar/baz") },      FPL("")},
    { { FPL("/foo/bar"),      FPL("/foo/bar2/baz") },     FPL("")},
    { { FPL("/foo/bar"),      FPL("/foo/bar") },          FPL("")},
    { { FPL("/foo/bar/baz"),  FPL("/foo/bar") },          FPL("")},
    { { FPL("foo/bar"),       FPL("foo/bar/baz") },       FPL("baz")},
    { { FPL("foo/bar"),       FPL("foo2/bar/baz") },      FPL("")},
    { { FPL("foo/bar"),       FPL("foo/bar2/baz") },      FPL("")},
    { { FPL(""),              FPL("foo") },               FPL("")},
  };

  const FilePath base(FPL("blah"));

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath parent(cases[i].inputs[0]);
    FilePath child(cases[i].inputs[1]);
    {
      FilePath result;
      bool success = parent.AppendRelativePath(child, &result);
      EXPECT_EQ(cases[i].expected[0] != '\0', success) <<
        "i: " << i << ", parent: " << parent.value() << ", child: " <<
        child.value();
      EXPECT_STREQ(cases[i].expected, result.value().c_str()) <<
        "i: " << i << ", parent: " << parent.value() << ", child: " <<
        child.value();
    }
    {
      FilePath result(base);
      bool success = parent.AppendRelativePath(child, &result);
      EXPECT_EQ(cases[i].expected[0] != '\0', success) <<
        "i: " << i << ", parent: " << parent.value() << ", child: " <<
        child.value();
      EXPECT_EQ(base.Append(cases[i].expected).value(), result.value()) <<
        "i: " << i << ", parent: " << parent.value() << ", child: " <<
        child.value();
    }
  }
}

TEST_F(FilePathTest, EqualityTest) {
  const struct BinaryBooleanTestData cases[] = {
    { { FPL("/foo/bar/baz"),  FPL("/foo/bar/baz") },      true},
    { { FPL("/foo/bar"),      FPL("/foo/bar/baz") },      false},
    { { FPL("/foo/bar/baz"),  FPL("/foo/bar") },          false},
    { { FPL("//foo/bar/"),    FPL("//foo/bar/") },        true},
    { { FPL("/foo/bar"),      FPL("/foo2/bar") },         false},
    { { FPL("/foo/bar.txt"),  FPL("/foo/bar") },          false},
    { { FPL("foo/bar"),       FPL("foo/bar") },           true},
    { { FPL("foo/bar"),       FPL("foo/bar/baz") },       false},
    { { FPL(""),              FPL("foo") },               false},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath a(cases[i].inputs[0]);
    FilePath b(cases[i].inputs[1]);

    EXPECT_EQ(a == b, cases[i].expected) <<
      "equality i: " << i << ", a: " << a.value() << ", b: " <<
      b.value();
  }

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath a(cases[i].inputs[0]);
    FilePath b(cases[i].inputs[1]);

    EXPECT_EQ(a != b, !cases[i].expected) <<
      "inequality i: " << i << ", a: " << a.value() << ", b: " <<
      b.value();
  }
}

TEST_F(FilePathTest, ReferencesParent) {
  const struct UnaryBooleanTestData cases[] = {
    { FPL("."),        false },
    { FPL(".."),       true },
    { FPL(".. "),      true },
    { FPL(" .."),      true },
    { FPL("..."),      true },
    { FPL("a.."),      false },
    { FPL("..a"),      false },
    { FPL("../"),      true },
    { FPL("/.."),      true },
    { FPL("/../"),     true },
    { FPL("/a../"),    false },
    { FPL("/..a/"),    false },
    { FPL("//.."),     true },
    { FPL("..//"),     true },
    { FPL("//..//"),   true },
    { FPL("a//..//c"), true },
    { FPL("../b/c"),   true },
    { FPL("/../b/c"),  true },
    { FPL("a/b/.."),   true },
    { FPL("a/b/../"),  true },
    { FPL("a/../c"),   true },
    { FPL("a/b/c"),    false },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    bool observed = input.ReferencesParent();
    EXPECT_EQ(cases[i].expected, observed) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, ConstructWithNUL) {
  // Assert FPS() works.
  ASSERT_EQ(3U, FPS("a\0b").length());

  // Test constructor strips '\0'
  FilePath path(FPS("a\0b"));
  EXPECT_EQ(1U, path.value().length());
  EXPECT_EQ(FPL("a"), path.value());
}

TEST_F(FilePathTest, AppendWithNUL) {
  // Assert FPS() works.
  ASSERT_EQ(3U, FPS("b\0b").length());

  // Test Append() strips '\0'
  FilePath path(FPL("a"));
  path = path.Append(FPS("b\0b"));
  EXPECT_EQ(3U, path.value().length());
  EXPECT_EQ(FPL("a/b"), path.value());
}

TEST_F(FilePathTest, ReferencesParentWithNUL) {
  // Assert FPS() works.
  ASSERT_EQ(3U, FPS("..\0").length());

  // Test ReferencesParent() doesn't break with "..\0"
  FilePath path(FPS("..\0"));
  EXPECT_TRUE(path.ReferencesParent());
}

TEST_F(FilePathTest, EndsWithSeparator) {
  const UnaryBooleanTestData cases[] = {
    { FPL(""), false },
    { FPL("/"), true },
    { FPL("foo/"), true },
    { FPL("bar"), false },
    { FPL("/foo/bar"), false },
  };
  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    EXPECT_EQ(cases[i].expected, input.EndsWithSeparator());
  }
}

TEST_F(FilePathTest, AsEndingWithSeparator) {
  const UnaryTestData cases[] = {
    { FPL(""), FPL("") },
    { FPL("/"), FPL("/") },
    { FPL("foo"), FPL("foo/") },
    { FPL("foo/"), FPL("foo/") }
  };
  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    EXPECT_EQ(cases[i].expected, input.AsEndingWithSeparator().value());
  }
}

// Test the PrintTo overload for FilePath (used when a test fails to compare two
// FilePaths).
TEST_F(FilePathTest, PrintTo) {
  std::stringstream ss;
  FilePath fp(FPL("foo"));
  PrintTo(fp, &ss);
  EXPECT_EQ("foo", ss.str());
}

}  // namespace files
