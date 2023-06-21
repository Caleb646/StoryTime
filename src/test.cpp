
#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <cassert>
#include <vector>

#include "types.h"
#include "utils.h"
#include "core.h"
#include "NDB.h"
#include "LTP.h"
#include "reader.h"

using namespace reader;
using namespace reader::ndb;
using namespace reader::ltp;
using namespace reader::core;

// BT page (cLevel=1), with 3 BTENTRY items (cEnt=3), 
// each of size 0x18 bytes (cbEnt=0x18), and 
// the maximum capacity of the page is 0x14 BTENTRY structures (cEntMax=0x14)
std::vector<types::byte_t> sample_btpage = {
   0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x41, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x22, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
   
   0x03, 0x14, 0x18, 0x01, 0x00, 0x00, 0x00, 0x00, // BTPage Structure
   0x81, 0x81, 0x06, 0x80, 0x64, 0xB1, 0xE8, 0x02, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Page trailer
};

std::vector<types::byte_t> sample_nbtentryPage = {

  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// nid = 1
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// bidData = 2
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// bidSub = 0
  0x00, 0x00, 0x00, 0x00,							// parentNid = 0	
  0x00, 0x00, 0x00, 0x00,							// dwPadding = 0

  0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// nid = 2
  0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// bidData = 4
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// bidSub = 0
  0x00, 0x00, 0x00, 0x00,							// parentNid = 0	
  0x00, 0x00, 0x00, 0x00,							// dwPadding = 0

  0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// nid = 5
  0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// bidData = 6
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// bidSub = 0
  0x00, 0x00, 0x00, 0x00,							// parentNid = 0	
  0x00, 0x00, 0x00, 0x00,							// dwPadding = 0

  0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// nid = 7
  0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// bidData = 8
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// bidSub = 0
  0x00, 0x00, 0x00, 0x00,							// parentNid = 0	
  0x00, 0x00, 0x00, 0x00,							// dwPadding = 0

  0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// nid = 9
  0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// bidData = 10
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// bidSub = 0
  0x00, 0x00, 0x00, 0x00,							// parentNid = 0	
  0x00, 0x00, 0x00, 0x00,							// dwPadding = 0

  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  
  0x05, 0x0F, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, // BTPage Structure
  0x81, 0x81, 0x6B, 0x70, 0x49, 0x19, 0xC2, 0x39, 0x6B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Page Trailer
};

std::vector<types::byte_t> sample_bbtentryPage = {
 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Bref
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 
 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Bref
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Bref
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 
 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Bref
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Bref
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
 
 0x05, 0x14, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, // BTPage Structure
 0x80, 0x80, 0xD6, 0x00, 0x2F, 0xA0, 0xF6, 0xA1, 0x46, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Page Trailer
};

std::vector<types::byte_t> sample_HN = {
	0xEC, 0x00, 0xEC, 0xBC, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB5, 0x02, 0x06, 0x00,
	0x40, 0x00, 0x00, 0x00, 0x34, 0x0E, 0x02, 0x01, 0xA0, 0x00, 0x00, 0x00, 0x38, 0x0E, 0x03, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xF9, 0x0F, 0x02, 0x01, 0x60, 0x00, 0x00, 0x00, 0x01, 0x30, 0x1F, 0x00,
	0x80, 0x00, 0x00, 0x00, 0xDF, 0x35, 0x03, 0x00, 0x89, 0x00, 0x00, 0x00, 0xE0, 0x35, 0x02, 0x01,
	0xC0, 0x00, 0x00, 0x00, 0xE3, 0x35, 0x02, 0x01, 0x00, 0x01, 0x00, 0x00, 0xE7, 0x35, 0x02, 0x01,
	0xE0, 0x00, 0x00, 0x00, 0x33, 0x66, 0x0B, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFA, 0x66, 0x03, 0x00,
	0x0D, 0x00, 0x0E, 0x00, 0xFF, 0x67, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x9D, 0xB5, 0x0A,
	0xDC, 0xD9, 0x94, 0x43, 0x85, 0xDE, 0x90, 0xAE, 0xB0, 0x7D, 0x12, 0x70, 0x55, 0x00, 0x4E, 0x00,
	0x49, 0x00, 0x43, 0x00, 0x4F, 0x00, 0x44, 0x00, 0x45, 0x00, 0x31, 0x00, 0x01, 0x00, 0x00, 0x00,
	0xF5, 0x5E, 0xF6, 0x66, 0x95, 0x69, 0xCC, 0x4C, 0x83, 0xD1, 0xD8, 0x73, 0x98, 0x99, 0x02, 0x85,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x9D, 0xB5, 0x0A, 0xDC, 0xD9, 0x94, 0x43,
	0x85, 0xDE, 0x90, 0xAE, 0xB0, 0x7D, 0x12, 0x70, 0x22, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x22, 0x9D, 0xB5, 0x0A, 0xDC, 0xD9, 0x94, 0x43, 0x85, 0xDE, 0x90, 0xAE, 0xB0, 0x7D, 0x12, 0x70,
	0x42, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x9D, 0xB5, 0x0A, 0xDC, 0xD9, 0x94, 0x43,
	0x85, 0xDE, 0x90, 0xAE, 0xB0, 0x7D, 0x12, 0x70, 0x62, 0x80, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
	0x0C, 0x00, 0x14, 0x00, 0x6C, 0x00, 0x7C, 0x00, 0x8C, 0x00, 0xA4, 0x00, 0xBC, 0x00, 0xD4, 0x00,
	0xEC, 0x00
};

int main()
{   
	BTPage btpage = BTPage::readBTPage(sample_btpage, types::PType::NBT);
	ASSERT((btpage.cLevel == 1), "[ERROR]");
	ASSERT((btpage.rgentries.size() == 3), "[ERROR]");
	ASSERT((btpage.cbEnt == 0x18), "[ERROR]");
	ASSERT((btpage.cEntMax == 0x14), "[ERROR]");
	ASSERT((btpage.hasBTEntries() == true), "[ERROR]");

	BTPage nbtpage = BTPage::readBTPage(sample_nbtentryPage, types::PType::NBT);
	ASSERT((nbtpage.cLevel == 0), "[ERROR]");
	ASSERT((nbtpage.rgentries.size() == 0x05), "[ERROR]");
	ASSERT((nbtpage.cEnt == 0x05), "[ERROR]");
	ASSERT((nbtpage.cbEnt == 0x20), "[ERROR]");
	ASSERT((nbtpage.cEntMax == 0x0F), "[ERROR]");
	ASSERT((nbtpage.hasNBTEntries() == true), "[ERROR]");

	uint32_t odd = 1;
	uint32_t even = 2;
	for (uint32_t i = 0; i < nbtpage.rgentries.size(); i++)
	{
		const NBTEntry& nbt = nbtpage.rgentries.at(i).asNBTEntry();
		ASSERT((nbt.nid == NID(odd)), "[ERROR]");
		ASSERT((nbt.bidData == BID(even)), "[ERROR]");
		ASSERT((nbt.bidSub == BID(0)), "[ERROR]");
		ASSERT((nbt.nidParent == NID(0)), "[ERROR]");
		odd += 2;
		even += 2;
	}

	BTPage bbtpage = BTPage::readBTPage(sample_bbtentryPage, types::PType::BBT);
	ASSERT((bbtpage.cLevel == 0), "[ERROR]");
	ASSERT((bbtpage.rgentries.size() == 5), "[ERROR]");
	ASSERT((bbtpage.cEnt == 5), "[ERROR]");
	ASSERT((bbtpage.cbEnt == 0x18), "[ERROR]");
	ASSERT((bbtpage.cEntMax == 0x14), "[ERROR]");
	ASSERT((bbtpage.hasBBTEntries() == true), "[ERROR]");

	even = 2;
	for (uint32_t i = 0; i < bbtpage.rgentries.size(); i++)
	{
		const BBTEntry& bbt = bbtpage.rgentries.at(i).asBBTEntry();
		ASSERT((bbt.bref.bid == BID(even)), "[ERROR]");
		even += 2;
	}

	BTree<NBTEntry> nodes(nbtpage);
	BTree<BBTEntry> blocks(bbtpage);

	for (const BTPage& page : nodes)
	{
		odd = 1;
		even = 2;
		for (const Entry& e : page.rgentries)
		{
			const NBTEntry& nbt = nodes.get(NID(odd));
			const BBTEntry& bbt = blocks.get(nbt.bidData);
			ASSERT((nbt.nid == NID(odd)), "[ERROR]");
			ASSERT((nbt.bidData == BID(even)), "[ERROR]");
			ASSERT((bbt.bref.bid == BID(even)), "[ERROR]");
			odd += 2;
			even += 2;
		}
	}

	HNHDR hnhdr = HN::readHNHDR(sample_HN, 0, 1);
	ASSERT((hnhdr.bSig == 0xEC), "[ERROR]");
	ASSERT((hnhdr.bClientSig == 0xBC), "[ERROR]");
	ASSERT((hnhdr.hidUserRoot.getHIDRaw() == 0x00000020), "[ERROR]");
	ASSERT((hnhdr.ibHnpm == 0x00EC), "[ERROR]");

	std::vector<types::byte_t> A(sample_HN);
	std::vector<types::byte_t> B(sample_HN);
	utils::ms::CryptPermute(A.data(), static_cast<int>(A.size()), utils::ms::ENCODE_DATA);
	ASSERT((A != B), "[ERROR]");
	utils::ms::CryptPermute(A.data(), static_cast<int>(A.size()), utils::ms::DECODE_DATA);
	ASSERT((A == B), "[ERROR]");

	return 0;
}
