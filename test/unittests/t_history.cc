#include <gtest/gtest.h>
#include <string>

#include "../../cvmfs/util.h"
#include "../../cvmfs/prng.h"
#include "../../cvmfs/history.h"

using namespace history;

class T_History : public ::testing::Test {
 protected:
  static const std::string sandbox;
  static const std::string fqrn;

  typedef std::vector<History::Tag> TagVector;

 protected:
  virtual void SetUp() {
    const bool retval = MkdirDeep(sandbox, 0700);
    ASSERT_TRUE (retval) << "failed to create sandbox";
    prng_.InitSeed(42);
  }

  virtual void TearDown() {
    const bool retval = RemoveTree(sandbox);
    ASSERT_TRUE (retval) << "failed to remove sandbox";
  }

  std::string GetHistoryFilename() const {
    const std::string path = CreateTempPath(sandbox + "/history", 0600);
    CheckEmpty(path);
    return path;
  }

  History::Tag GetDummyTag(
        const std::string            &name      = "foobar",
        const uint64_t                revision  = 42,
        const History::UpdateChannel  channel   = History::kChannelTest,
        const time_t                  timestamp = 564993000) const {
      shash::Any root_hash(shash::kSha1);
      root_hash.Randomize();

      History::Tag dummy;
      dummy.name        = name;
      dummy.root_hash   = root_hash;
      dummy.size        = 1337;
      dummy.revision    = revision;
      dummy.timestamp   = timestamp;
      dummy.channel     = channel;
      dummy.description = "This is just a small dummy";

      return dummy;
  }

  TagVector GetDummyTags(const unsigned int count) const {
    TagVector result;
    result.reserve(count);
    for (unsigned int i = 0; i < count; ++i) {
      shash::Any root_hash(shash::kSha1);
      root_hash.Randomize();

      History::Tag dummy;
      dummy.name        = "dummy" + StringifyInt(i);
      dummy.root_hash   = root_hash;
      dummy.size        = prng_.Next(1024);
      dummy.revision    = i;
      dummy.timestamp   = prng_.Next(564993000);
      dummy.channel     = History::kChannelDevel;
      dummy.description = "This is just a small dummy with number " +
                          StringifyInt(i);

      result.push_back(dummy);
    }

    return result;
  }

  bool CheckListing(const TagVector &lhs, const TagVector &rhs) const {
    if (lhs.size() != rhs.size()) {
      return false;
    }

          TagVector::const_iterator i    = lhs.begin();
    const TagVector::const_iterator iend = lhs.end();
    for (; i != iend; ++i) {
      bool found = false;
            TagVector::const_iterator j    = rhs.begin();
      const TagVector::const_iterator jend = rhs.end();
      for (; j != jend; ++j) {
        if (TagsEqual(*i, *j)) {
          found = true;
          break;
        }
      }

      if (! found) {
        return false;
      }
    }

    return true;
  }

  bool TagsEqual(const History::Tag &lhs, const History::Tag &rhs) const {
    return (lhs.name        == rhs.name)        &&
           (lhs.root_hash   == rhs.root_hash)   &&
           (lhs.size        == rhs.size)        &&
           (lhs.revision    == rhs.revision)    &&
           (lhs.timestamp   == rhs.timestamp)   &&
           (lhs.channel     == rhs.channel)     &&
           (lhs.description == rhs.description);
  }

  void CompareTags(const History::Tag &lhs, const History::Tag &rhs) const {
    EXPECT_EQ (lhs.name,        rhs.name);
    EXPECT_EQ (lhs.root_hash,   rhs.root_hash);
    EXPECT_EQ (lhs.size,        rhs.size);
    EXPECT_EQ (lhs.revision,    rhs.revision);
    EXPECT_EQ (lhs.timestamp,   rhs.timestamp);
    EXPECT_EQ (lhs.channel,     rhs.channel);
    EXPECT_EQ (lhs.description, rhs.description);
  }

 private:
  void CheckEmpty(const std::string &str) const {
    ASSERT_FALSE (str.empty());
  }

 private:
  mutable Prng prng_;
};

const std::string T_History::sandbox = "/tmp/cvmfs_ut_history";
const std::string T_History::fqrn    = "test.cern.ch";


TEST_F(T_History, Initialize) {}


TEST_F(T_History, CreateHistory) {
  History *history = History::Create(GetHistoryFilename(), fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history);
  EXPECT_EQ (fqrn, history->fqrn());
  delete history;
}


TEST_F(T_History, OpenHistory) {
  const std::string hp = GetHistoryFilename();
  History *history1 = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history1);
  EXPECT_EQ (fqrn, history1->fqrn());
  delete history1;

  History *history2 = History::Open(hp);
  ASSERT_NE (static_cast<History*>(NULL), history2);
  EXPECT_EQ (fqrn, history2->fqrn());
  delete history2;
}


TEST_F(T_History, InsertTag) {
  const std::string hp = GetHistoryFilename();
  History *history = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history);
  EXPECT_EQ (fqrn, history->fqrn());
  ASSERT_TRUE (history->Insert(GetDummyTag()));
  EXPECT_EQ (1, history->GetNumberOfTags());
  delete history;
}


TEST_F(T_History, InsertTwice) {
  const std::string hp = GetHistoryFilename();
  History *history = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history);
  EXPECT_EQ (fqrn, history->fqrn());
  ASSERT_TRUE  (history->Insert(GetDummyTag()));
  EXPECT_EQ (1, history->GetNumberOfTags());
  ASSERT_FALSE (history->Insert(GetDummyTag()));
  EXPECT_EQ (1, history->GetNumberOfTags());
  delete history;
}


TEST_F(T_History, CountTags) {
  const std::string hp = GetHistoryFilename();
  History *history = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history);
  EXPECT_EQ (fqrn, history->fqrn());

  const unsigned int dummy_count = 1000;
  const TagVector dummy_tags = GetDummyTags(dummy_count);
  ASSERT_TRUE (history->BeginTransaction());
        TagVector::const_iterator i    = dummy_tags.begin();
  const TagVector::const_iterator iend = dummy_tags.end();
  for (; i != iend; ++i) {
    ASSERT_TRUE (history->Insert(*i));
  }
  EXPECT_TRUE (history->CommitTransaction());

  EXPECT_EQ (dummy_count, history->GetNumberOfTags());

  delete history;
}


TEST_F(T_History, InsertAndFindTag) {
  const std::string hp = GetHistoryFilename();
  History *history = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history);
  EXPECT_EQ (fqrn, history->fqrn());
  History::Tag dummy = GetDummyTag();
  EXPECT_TRUE (history->Insert(dummy));
  EXPECT_EQ (1, history->GetNumberOfTags());

  History::Tag tag;
  ASSERT_TRUE (history->GetByName(dummy.name, &tag));
  CompareTags (dummy, tag);

  delete history;
}


TEST_F(T_History, InsertReopenAndFindTag) {
  const std::string hp = GetHistoryFilename();
  History *history1 = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history1);
  EXPECT_EQ (fqrn, history1->fqrn());
  History::Tag dummy = GetDummyTag();
  EXPECT_TRUE (history1->Insert(dummy));
  EXPECT_EQ (1, history1->GetNumberOfTags());

  History::Tag tag1;
  ASSERT_TRUE (history1->GetByName(dummy.name, &tag1));
  CompareTags (dummy, tag1);
  delete history1;

  History *history2 = History::Open(hp);
  ASSERT_NE (static_cast<History*>(NULL), history2);
  EXPECT_EQ (fqrn, history2->fqrn());

  History::Tag tag2;
  ASSERT_TRUE (history2->GetByName(dummy.name, &tag2));
  CompareTags (dummy, tag2);
  delete history2;
}


TEST_F(T_History, ListTags) {
  const std::string hp = GetHistoryFilename();
  History *history = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history);
  EXPECT_EQ (fqrn, history->fqrn());

  const unsigned int dummy_count = 1000;
  const TagVector dummy_tags = GetDummyTags(dummy_count);
  ASSERT_TRUE (history->BeginTransaction());
        TagVector::const_iterator i    = dummy_tags.begin();
  const TagVector::const_iterator iend = dummy_tags.end();
  for (; i != iend; ++i) {
    ASSERT_TRUE (history->Insert(*i));
  }
  EXPECT_TRUE (history->CommitTransaction());

  EXPECT_EQ (dummy_count, history->GetNumberOfTags());

  TagVector tags;
  ASSERT_TRUE (history->List(&tags));
  EXPECT_EQ (dummy_count, tags.size());

                                          i    = dummy_tags.begin();
        TagVector::const_reverse_iterator j    = tags.rbegin();
  const TagVector::const_reverse_iterator jend = tags.rend();
  for (; j != jend; ++j, ++i) {
    CompareTags(*i, *j);
  }

  delete history;
}


TEST_F(T_History, InsertAndRemoveTag) {
  const std::string hp = GetHistoryFilename();
  History *history = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history);
  EXPECT_EQ (fqrn, history->fqrn());

  const unsigned int dummy_count = 40;
  const TagVector dummy_tags = GetDummyTags(dummy_count);
  ASSERT_TRUE (history->BeginTransaction());
        TagVector::const_iterator i    = dummy_tags.begin();
  const TagVector::const_iterator iend = dummy_tags.end();
  for (; i != iend; ++i) {
    ASSERT_TRUE (history->Insert(*i));
  }
  EXPECT_TRUE (history->CommitTransaction());
  EXPECT_EQ (dummy_count, history->GetNumberOfTags());

  const std::string to_be_deleted = dummy_tags[5].name;
  EXPECT_TRUE (history->Exists(to_be_deleted));
  ASSERT_TRUE (history->Remove(dummy_tags[5].name));
  EXPECT_EQ (dummy_count - 1, history->GetNumberOfTags());
  EXPECT_FALSE (history->Exists(to_be_deleted));

  TagVector tags;
  ASSERT_TRUE (history->List(&tags));
  EXPECT_EQ (dummy_count - 1, tags.size());

                                          i    = dummy_tags.begin();
        TagVector::const_reverse_iterator j    = tags.rbegin();
  const TagVector::const_reverse_iterator jend = tags.rend();
  for (; j != jend; ++j, ++i) {
    if (i->name == to_be_deleted) {
      --j;
      continue;
    }
    CompareTags(*i, *j);
  }

  delete history;
}


TEST_F(T_History, RemoveNonExistentTag) {
  const std::string hp = GetHistoryFilename();
  History *history = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history);
  EXPECT_EQ (fqrn, history->fqrn());

  const unsigned int dummy_count = 40;
  const TagVector dummy_tags = GetDummyTags(dummy_count);
  ASSERT_TRUE (history->BeginTransaction());
        TagVector::const_iterator i    = dummy_tags.begin();
  const TagVector::const_iterator iend = dummy_tags.end();
  for (; i != iend; ++i) {
    ASSERT_TRUE (history->Insert(*i));
  }
  EXPECT_TRUE (history->CommitTransaction());
  EXPECT_EQ (dummy_count, history->GetNumberOfTags());

  ASSERT_TRUE (history->Remove("doesnt_exist"));
  EXPECT_EQ (dummy_count, history->GetNumberOfTags());

  TagVector tags;
  ASSERT_TRUE (history->List(&tags));
  EXPECT_EQ (dummy_count, tags.size());

                                          i    = dummy_tags.begin();
        TagVector::const_reverse_iterator j    = tags.rbegin();
  const TagVector::const_reverse_iterator jend = tags.rend();
  for (; j != jend; ++j, ++i) {
    CompareTags(*i, *j);
  }

  delete history;
}


TEST_F(T_History, RemoveMultipleTags) {
  const std::string hp = GetHistoryFilename();
  History *history = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history);
  EXPECT_EQ (fqrn, history->fqrn());

  const unsigned int dummy_count = 40;
  const TagVector dummy_tags = GetDummyTags(dummy_count);
  ASSERT_TRUE (history->BeginTransaction());
        TagVector::const_iterator i    = dummy_tags.begin();
  const TagVector::const_iterator iend = dummy_tags.end();
  for (; i != iend; ++i) {
    ASSERT_TRUE (history->Insert(*i));
  }
  EXPECT_TRUE (history->CommitTransaction());
  EXPECT_EQ (dummy_count, history->GetNumberOfTags());

  std::vector<std::string> to_be_deleted;
  to_be_deleted.push_back(dummy_tags[2].name);
  to_be_deleted.push_back(dummy_tags[5].name);
  to_be_deleted.push_back(dummy_tags[10].name);
  to_be_deleted.push_back(dummy_tags[15].name);

        std::vector<std::string>::const_iterator j    = to_be_deleted.begin();
  const std::vector<std::string>::const_iterator jend = to_be_deleted.end();
  for (; j != jend; ++j) {
    EXPECT_TRUE (history->Remove(*j));
  }
  EXPECT_EQ (dummy_count - to_be_deleted.size(), history->GetNumberOfTags());

  TagVector tags;
  ASSERT_TRUE (history->List(&tags));
  EXPECT_EQ (dummy_count - to_be_deleted.size(), tags.size());

                                          i    = dummy_tags.begin();
        TagVector::const_reverse_iterator k    = tags.rbegin();
  const TagVector::const_reverse_iterator kend = tags.rend();
  for (; k != kend; ++k, ++i) {
    bool should_exist = true;
          std::vector<std::string>::const_iterator l    = to_be_deleted.begin();
    const std::vector<std::string>::const_iterator lend = to_be_deleted.end();
    for (; l != lend; ++l) {
      if (i->name == *l) {
        --k;
        should_exist = false;
        break;
      }
    }
    if (should_exist) {
      CompareTags(*i, *k);
    }
  }

  delete history;
}


TEST_F(T_History, RemoveTagsWithReOpen) {
  const std::string hp = GetHistoryFilename();
  History *history1 = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history1);
  EXPECT_EQ (fqrn, history1->fqrn());

  const unsigned int dummy_count = 40;
  const TagVector dummy_tags = GetDummyTags(dummy_count);
  ASSERT_TRUE (history1->BeginTransaction());
        TagVector::const_iterator i    = dummy_tags.begin();
  const TagVector::const_iterator iend = dummy_tags.end();
  for (; i != iend; ++i) {
    ASSERT_TRUE (history1->Insert(*i));
  }
  EXPECT_TRUE (history1->CommitTransaction());
  EXPECT_EQ (dummy_count, history1->GetNumberOfTags());
  delete history1;

  History *history2 = History::OpenWritable(hp);
  ASSERT_NE (static_cast<History*>(NULL), history2);
  EXPECT_EQ (fqrn, history2->fqrn());

  std::vector<std::string> to_be_deleted;
  to_be_deleted.push_back(dummy_tags[2].name);
  to_be_deleted.push_back(dummy_tags[5].name);
  to_be_deleted.push_back(dummy_tags[10].name);
  to_be_deleted.push_back(dummy_tags[15].name);

        std::vector<std::string>::const_iterator j    = to_be_deleted.begin();
  const std::vector<std::string>::const_iterator jend = to_be_deleted.end();
  for (; j != jend; ++j) {
    EXPECT_TRUE (history2->Remove(*j));
  }
  EXPECT_EQ (dummy_count - to_be_deleted.size(), history2->GetNumberOfTags());
  delete history2;

  History *history3 = History::Open(hp);
  TagVector tags;
  ASSERT_TRUE (history3->List(&tags));
  EXPECT_EQ (dummy_count - to_be_deleted.size(), tags.size());

                                          i    = dummy_tags.begin();
        TagVector::const_reverse_iterator k    = tags.rbegin();
  const TagVector::const_reverse_iterator kend = tags.rend();
  for (; k != kend; ++k, ++i) {
    bool should_exist = true;
          std::vector<std::string>::const_iterator l    = to_be_deleted.begin();
    const std::vector<std::string>::const_iterator lend = to_be_deleted.end();
    for (; l != lend; ++l) {
      if (i->name == *l) {
        --k;
        should_exist = false;
        break;
      }
    }
    if (should_exist) {
      CompareTags(*i, *k);
    }
  }

  delete history3;
}


TEST_F(T_History, GetChannelTips) {
  const std::string hp = GetHistoryFilename();
  History *history1 = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history1);
  EXPECT_EQ (fqrn, history1->fqrn());

  history1->BeginTransaction();
  const History::Tag trunk_tip = GetDummyTag("zap", 4, History::kChannelTrunk);
  ASSERT_TRUE (history1->Insert(GetDummyTag("foo",  1, History::kChannelTrunk)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("bar",  2, History::kChannelTrunk)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("baz",  3, History::kChannelTrunk)));
  ASSERT_TRUE (history1->Insert(trunk_tip));

  const History::Tag test_tip = GetDummyTag("yolo",   6, History::kChannelTest);
  ASSERT_TRUE (history1->Insert(GetDummyTag("moep",   3, History::kChannelTest)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("lol",    4, History::kChannelTest)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("cheers", 5, History::kChannelTest)));
  ASSERT_TRUE (history1->Insert(test_tip));
  history1->CommitTransaction();

  TagVector tags;
  ASSERT_TRUE (history1->Tips(&tags));
  EXPECT_EQ (2u, tags.size());

  TagVector expected; // TODO: C++11 initializer lists
  expected.push_back(trunk_tip);
  expected.push_back(test_tip);
  EXPECT_TRUE (CheckListing(tags, expected));

  history1->BeginTransaction();
  const History::Tag prod_tip = GetDummyTag("prod", 10, History::kChannelProd);
  ASSERT_TRUE (history1->Insert(GetDummyTag("vers", 3, History::kChannelProd)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("bug",  6, History::kChannelProd)));
  ASSERT_TRUE (history1->Insert(prod_tip));
  history1->CommitTransaction();

  tags.clear();
  ASSERT_TRUE (history1->Tips(&tags));
  EXPECT_EQ (3u, tags.size());

  expected.push_back(prod_tip);
  EXPECT_TRUE (CheckListing(tags, expected));

  delete history1;

  History *history2 = History::Open(hp);
  ASSERT_NE (static_cast<History*>(NULL), history2);
  EXPECT_EQ (fqrn, history2->fqrn());

  tags.clear();
  ASSERT_TRUE (history2->Tips(&tags));
  EXPECT_EQ   (3u, tags.size());
  EXPECT_TRUE (CheckListing(tags, expected));

  delete history2;
}


TEST_F(T_History, GetHashes) {
  const std::string hp = GetHistoryFilename();
  History *history = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history);
  EXPECT_EQ (fqrn, history->fqrn());

  const unsigned int dummy_count = 1000;
  const TagVector dummy_tags = GetDummyTags(dummy_count);
  ASSERT_TRUE (history->BeginTransaction());
        TagVector::const_reverse_iterator i    = dummy_tags.rbegin();
  const TagVector::const_reverse_iterator iend = dummy_tags.rend();
  for (; i != iend; ++i) {
    ASSERT_TRUE (history->Insert(*i));
  }
  EXPECT_TRUE (history->CommitTransaction());

  EXPECT_EQ (dummy_count, history->GetNumberOfTags());

  std::vector<shash::Any> hashes;
  ASSERT_TRUE (history->GetHashes(&hashes));

        TagVector::const_iterator j    = dummy_tags.begin();
  const TagVector::const_iterator jend = dummy_tags.end();
        std::vector<shash::Any>::const_iterator k    = hashes.begin();
  const std::vector<shash::Any>::const_iterator kend = hashes.end();
  ASSERT_EQ (dummy_tags.size(), hashes.size());
  for (; j != jend; ++j, ++k) {
    EXPECT_EQ (j->root_hash, *k);
  }

  delete history;
}


TEST_F(T_History, GetTagByDate) {
  const std::string hp = GetHistoryFilename();
  History *history = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history);
  EXPECT_EQ (fqrn, history->fqrn());

  const History::UpdateChannel c = History::kChannelTest;
  const History::Tag t3010 = GetDummyTag("f5", 1, c, 1414690911);
  const History::Tag t3110 = GetDummyTag("f4", 2, c, 1414777311);
  const History::Tag t0111 = GetDummyTag("f3", 3, c, 1414863711);
  const History::Tag t0211 = GetDummyTag("f2", 4, c, 1414950111);
  const History::Tag t0311 = GetDummyTag("f1", 5, c, 1415036511);

  history->BeginTransaction();
  ASSERT_TRUE (history->Insert(t0311));
  ASSERT_TRUE (history->Insert(t0211));
  ASSERT_TRUE (history->Insert(t0111));
  ASSERT_TRUE (history->Insert(t3110));
  ASSERT_TRUE (history->Insert(t3010));
  history->CommitTransaction();

  const time_t ts2510 = 1414255311;
  const time_t ts0111 = 1414864111;
  const time_t ts3110 = 1414777311;
  const time_t ts0411 = 1415126511;

  History::Tag tag;
  EXPECT_FALSE (history->GetByDate(ts2510, &tag)); // No revision yet

  EXPECT_TRUE (history->GetByDate(ts3110, &tag));
  CompareTags(t3110, tag);

  EXPECT_TRUE (history->GetByDate(ts0111, &tag));
  CompareTags(t0111, tag);

  EXPECT_TRUE (history->GetByDate(ts0411, &tag));
  CompareTags(t0311, tag);

  delete history;
}


TEST_F(T_History, RollbackToOldTag) {
  const std::string hp = GetHistoryFilename();
  History *history1 = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history1);
  EXPECT_EQ (fqrn, history1->fqrn());

  const History::UpdateChannel c_test = History::kChannelTest;
  const History::UpdateChannel c_prod = History::kChannelProd;

  ASSERT_TRUE (history1->BeginTransaction());
  ASSERT_TRUE (history1->Insert(GetDummyTag("foo",            1, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("bar",            2, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("first_release",  3, c_prod)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("moep",           4, c_test))); // <--
  ASSERT_TRUE (history1->Insert(GetDummyTag("moep_duplicate", 4, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("lol",            5, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("second_release", 6, c_prod)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("third_release",  7, c_prod)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("rofl",           8, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("also_rofl",      8, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("forth_release",  9, c_prod)));
  ASSERT_TRUE (history1->CommitTransaction());

  delete history1;

  History *history2 = History::OpenWritable(hp);
  ASSERT_NE (static_cast<History*>(NULL), history2);
  EXPECT_EQ (fqrn, history2->fqrn());

  ASSERT_TRUE (history2->BeginTransaction());
  History::Tag rollback_target;
  EXPECT_TRUE (history2->GetByName("moep", &rollback_target));

  TagVector gone;
  EXPECT_TRUE (history2->ListTagsAffectedByRollback("moep", &gone));
  ASSERT_EQ (4u, gone.size());
  if (gone[0].name == "also_rofl") { // order of rev 8 tags is undefined
    EXPECT_EQ ("also_rofl", gone[0].name); EXPECT_EQ (8, gone[0].revision);
    EXPECT_EQ ("rofl",      gone[1].name); EXPECT_EQ (8, gone[1].revision);
  } else {
    EXPECT_EQ ("rofl",      gone[0].name); EXPECT_EQ (8, gone[0].revision);
    EXPECT_EQ ("also_rofl", gone[1].name); EXPECT_EQ (8, gone[1].revision);
  }
  EXPECT_EQ ("lol",       gone[2].name); EXPECT_EQ (5, gone[2].revision);
  EXPECT_EQ ("moep",      gone[3].name); EXPECT_EQ (4, gone[3].revision);

  shash::Any new_root_hash(shash::kSha1);
  new_root_hash.Randomize();
  rollback_target.revision  = 10;
  rollback_target.root_hash = new_root_hash;

  EXPECT_TRUE (history2->Rollback(rollback_target));
  ASSERT_TRUE (history2->CommitTransaction());

  EXPECT_TRUE  (history2->Exists("foo"));
  EXPECT_TRUE  (history2->Exists("bar"));
  EXPECT_TRUE  (history2->Exists("first_release"));
  EXPECT_TRUE  (history2->Exists("moep"));
  EXPECT_TRUE  (history2->Exists("moep_duplicate"));
  EXPECT_TRUE  (history2->Exists("second_release"));
  EXPECT_TRUE  (history2->Exists("third_release"));
  EXPECT_TRUE  (history2->Exists("forth_release"));
  EXPECT_FALSE (history2->Exists("lol"));
  EXPECT_FALSE (history2->Exists("rofl"));
  EXPECT_FALSE (history2->Exists("also_rofl"));

  History::Tag rolled_back_tag;
  ASSERT_TRUE (history2->GetByName("moep", &rolled_back_tag));
  EXPECT_EQ (10u,           rolled_back_tag.revision);
  EXPECT_EQ (new_root_hash, rolled_back_tag.root_hash);

  delete history2;

  History *history3 = History::OpenWritable(hp);
  ASSERT_NE (static_cast<History*>(NULL), history3);
  EXPECT_EQ (fqrn, history3->fqrn());

  ASSERT_TRUE (history3->BeginTransaction());
  History::Tag rollback_target_malicious;
  EXPECT_TRUE (history3->GetByName("bar", &rollback_target_malicious));

  rollback_target_malicious.name      = "barlol";
  rollback_target_malicious.revision  = 11;
  EXPECT_FALSE (history3->Rollback(rollback_target_malicious));
  ASSERT_TRUE  (history3->CommitTransaction());

  EXPECT_TRUE  (history3->Exists("foo"));
  EXPECT_TRUE  (history3->Exists("bar"));
  EXPECT_TRUE  (history3->Exists("first_release"));
  EXPECT_TRUE  (history3->Exists("moep"));
  EXPECT_TRUE  (history2->Exists("moep_duplicate"));
  EXPECT_TRUE  (history3->Exists("second_release"));
  EXPECT_TRUE  (history3->Exists("third_release"));
  EXPECT_TRUE  (history3->Exists("forth_release"));
  EXPECT_FALSE (history3->Exists("lol"));
  EXPECT_FALSE (history3->Exists("rofl"));
  EXPECT_FALSE (history3->Exists("also_rofl"));

  delete history3;
}


TEST_F(T_History, ListTagsAffectedByRollback) {
  const std::string hp = GetHistoryFilename();
  History *history1 = History::Create(hp, fqrn);
  ASSERT_NE (static_cast<History*>(NULL), history1);
  EXPECT_EQ (fqrn, history1->fqrn());

  const History::UpdateChannel c_test = History::kChannelTest;
  const History::UpdateChannel c_prod = History::kChannelProd;

  ASSERT_TRUE (history1->BeginTransaction());
  ASSERT_TRUE (history1->Insert(GetDummyTag("foo",            1, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("bar",            2, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("first_release",  3, c_prod)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("test_release",   3, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("moep",           4, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("moep_duplicate", 4, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("lol",            5, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("second_release", 6, c_prod)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("third_release",  7, c_prod)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("rofl",           8, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("also_rofl",      8, c_test)));
  ASSERT_TRUE (history1->Insert(GetDummyTag("forth_release",  9, c_prod)));
  ASSERT_TRUE (history1->CommitTransaction());

  TagVector gone;
  EXPECT_TRUE (history1->ListTagsAffectedByRollback("moep",  &gone));
  ASSERT_EQ (4u, gone.size());
  if (gone[0].name == "also_rofl") { // order of rev 8 tags is undefined
    EXPECT_EQ ("also_rofl", gone[0].name); EXPECT_EQ (8, gone[0].revision);
    EXPECT_EQ ("rofl",      gone[1].name); EXPECT_EQ (8, gone[1].revision);
  } else {
    EXPECT_EQ ("rofl",      gone[0].name); EXPECT_EQ (8, gone[0].revision);
    EXPECT_EQ ("also_rofl", gone[1].name); EXPECT_EQ (8, gone[1].revision);
  }
  EXPECT_EQ ("lol",       gone[2].name); EXPECT_EQ (5, gone[2].revision);
  EXPECT_EQ ("moep",      gone[3].name); EXPECT_EQ (4, gone[3].revision);

  gone.clear();
  EXPECT_FALSE (history1->ListTagsAffectedByRollback("unobtainium", &gone));
  EXPECT_TRUE  (gone.empty());

  gone.clear();
  EXPECT_TRUE (history1->ListTagsAffectedByRollback("second_release", &gone));
  ASSERT_EQ (3u, gone.size());
  EXPECT_EQ ("forth_release",  gone[0].name); EXPECT_EQ (9, gone[0].revision);
  EXPECT_EQ ("third_release",  gone[1].name); EXPECT_EQ (7, gone[1].revision);
  EXPECT_EQ ("second_release", gone[2].name); EXPECT_EQ (6, gone[2].revision);

  gone.clear();
  EXPECT_TRUE (history1->ListTagsAffectedByRollback("bar", &gone));
  ASSERT_EQ (7u, gone.size());
  if (gone[0].name == "also_rofl") { // undefined order for same revision
    EXPECT_EQ ("also_rofl",      gone[0].name); EXPECT_EQ (8, gone[0].revision);
    EXPECT_EQ ("rofl",           gone[1].name); EXPECT_EQ (8, gone[1].revision);
  } else {
    EXPECT_EQ ("rofl",           gone[0].name); EXPECT_EQ (8, gone[0].revision);
    EXPECT_EQ ("also_rofl",      gone[1].name); EXPECT_EQ (8, gone[1].revision);
  }
  EXPECT_EQ ("lol",            gone[2].name); EXPECT_EQ (5, gone[2].revision);
  if (gone[3].name == "moep_duplicate") { // undefined order of same revision
    EXPECT_EQ ("moep_duplicate", gone[3].name); EXPECT_EQ (4, gone[3].revision);
    EXPECT_EQ ("moep",           gone[4].name); EXPECT_EQ (4, gone[4].revision);
  } else {
    EXPECT_EQ ("moep",           gone[3].name); EXPECT_EQ (4, gone[3].revision);
    EXPECT_EQ ("moep_duplicate", gone[4].name); EXPECT_EQ (4, gone[4].revision);
  }
  EXPECT_EQ ("test_release",   gone[5].name); EXPECT_EQ (3, gone[5].revision);
  EXPECT_EQ ("bar",            gone[6].name); EXPECT_EQ (2, gone[6].revision);

  gone.clear();
  EXPECT_TRUE (history1->ListTagsAffectedByRollback("forth_release", &gone));
  ASSERT_EQ (1u, gone.size());
  EXPECT_EQ ("forth_release", gone[0].name); EXPECT_EQ (9, gone[0].revision);

  delete history1;
}