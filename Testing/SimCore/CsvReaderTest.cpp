/* -*- mode: c++ -*- */
/****************************************************************************
 *****                                                                  *****
 *****                   Classification: UNCLASSIFIED                   *****
 *****                    Classified By:                                *****
 *****                    Declassify On:                                *****
 *****                                                                  *****
 ****************************************************************************
 *
 *
 * Developed by: Naval Research Laboratory, Tactical Electronic Warfare Div.
 *               EW Modeling & Simulation, Code 5773
 *               4555 Overlook Ave.
 *               Washington, D.C. 20375-5339
 *
 * License for source code at https://simdis.nrl.navy.mil/License.aspx
 *
 * The U.S. Government retains all rights to use, duplicate, distribute,
 * disclose, or release this software.
 *
 */
#include <sstream>
#include "simCore/Common/SDKAssert.h"
#include "simCore/String/CsvReader.h"

namespace {

int testCsvReadLine()
{
  int rv = 0;

  std::istringstream stream("one,two,three\nfour,five,six");

  simCore::CsvReader reader(stream);
  std::vector<std::string> tokens;

  // Test basic stream
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == "one");
  rv += SDK_ASSERT(tokens[1] == "two");
  rv += SDK_ASSERT(tokens[2] == "three");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == "four");
  rv += SDK_ASSERT(tokens[1] == "five");
  rv += SDK_ASSERT(tokens[2] == "six");
  rv += SDK_ASSERT(reader.readLine(tokens) == 1);

  // Test rows of differing lengths
  stream.clear();
  stream.str("one,two\nthree,four,five\nsix,seven");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 2);
  rv += SDK_ASSERT(tokens[0] == "one");
  rv += SDK_ASSERT(tokens[1] == "two");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == "three");
  rv += SDK_ASSERT(tokens[1] == "four");
  rv += SDK_ASSERT(tokens[2] == "five");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 2);
  rv += SDK_ASSERT(tokens[0] == "six");
  rv += SDK_ASSERT(tokens[1] == "seven");
  rv += SDK_ASSERT(reader.readLine(tokens) == 1);

  // Test stream with empty lines
  stream.clear();
  stream.str("one,two\n   \nthree,four,five\n  \nsix,seven");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 2);
  rv += SDK_ASSERT(tokens[0] == "one");
  rv += SDK_ASSERT(tokens[1] == "two");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == "three");
  rv += SDK_ASSERT(tokens[1] == "four");
  rv += SDK_ASSERT(tokens[2] == "five");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 2);
  rv += SDK_ASSERT(tokens[0] == "six");
  rv += SDK_ASSERT(tokens[1] == "seven");
  rv += SDK_ASSERT(reader.readLine(tokens) == 1);

  // Test basic stream with odd whitespace thrown in
  stream.clear();
  stream.str("one  , two,thr  ee\n four ,   five,six");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == "one  ");
  rv += SDK_ASSERT(tokens[1] == " two");
  rv += SDK_ASSERT(tokens[2] == "thr  ee");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == " four ");
  rv += SDK_ASSERT(tokens[1] == "   five");
  rv += SDK_ASSERT(tokens[2] == "six");
  rv += SDK_ASSERT(reader.readLine(tokens) == 1);

  return rv;
}

int testCsvReadLineTrimmed()
{
  int rv = 0;

  // Same leading and trailing whitespace test cases from testCsvReadLine(), but using readLineTrimmed
  std::istringstream stream("one  , two,thr  ee\n four ,   five,six");

  simCore::CsvReader reader(stream);
  std::vector<std::string> tokens;

  // Test basic stream
  rv += SDK_ASSERT(reader.readLineTrimmed(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == "one");
  rv += SDK_ASSERT(tokens[1] == "two");
  rv += SDK_ASSERT(tokens[2] == "thr  ee");
  rv += SDK_ASSERT(reader.readLineTrimmed(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == "four");
  rv += SDK_ASSERT(tokens[1] == "five");
  rv += SDK_ASSERT(tokens[2] == "six");
  rv += SDK_ASSERT(reader.readLineTrimmed(tokens) == 1);

  return 0;
}

int testCsvWithComments()
{
  int rv = 0;

  std::istringstream stream("#column 1, column 2, column 3\none,two,three\nfour,five,six");

  simCore::CsvReader reader(stream);
  std::vector<std::string> tokens;

  // Test comments
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == "one");
  rv += SDK_ASSERT(tokens[1] == "two");
  rv += SDK_ASSERT(tokens[2] == "three");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == "four");
  rv += SDK_ASSERT(tokens[1] == "five");
  rv += SDK_ASSERT(tokens[2] == "six");
  rv += SDK_ASSERT(reader.readLine(tokens) == 1);

  // Test changing the comment char
  reader.setCommentChar('$');
  stream.clear();
  stream.str("$column 1, column 2, column 3\none,two,three\nfour,five,six");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == "one");
  rv += SDK_ASSERT(tokens[1] == "two");
  rv += SDK_ASSERT(tokens[2] == "three");
  rv += SDK_ASSERT(reader.readLine(tokens) == 0);
  rv += SDK_ASSERT(tokens.size() == 3);
  rv += SDK_ASSERT(tokens[0] == "four");
  rv += SDK_ASSERT(tokens[1] == "five");
  rv += SDK_ASSERT(tokens[2] == "six");
  rv += SDK_ASSERT(reader.readLine(tokens) == 1);

  return rv;
}

}

int CsvReaderTest(int argc, char *argv[])
{
  int rv = 0;

  rv += SDK_ASSERT(testCsvReadLine() == 0);
  rv += SDK_ASSERT(testCsvReadLineTrimmed() == 0);
  rv += SDK_ASSERT(testCsvWithComments() == 0);

  return rv;
}
