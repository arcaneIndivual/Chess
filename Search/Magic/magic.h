#include "iostream"
#include "fstream"
#include "vector"
#include "map"
#include "../../Board/board.h"

class MagicMan{//magic manager, or at least thats my justification
  //the actual magic numbers
  u64 rookMagics[64];
  int rookShifts[64];
  u64 bishopMagics[64];
  int bishopShifts[64];

  //used for generating magic numbers 
  std::vector<u64> rookBlockers[64];
  std::vector<u64> bishopBlockers[64];

  bool testMagic(std::vector<u64> *blockers, int square, u64 magic, int shift);
  void generateBlockersFromMask(u64 mask,std::vector<u64> &target);
  void generateRookBlockers();
  void generateBishopBlockers();

  //using magic numbers
  void generateRookMasks();
  void generateBishopMasks();
  void fillRookMoves();
  void fillBishopMoves();
  std::map<u64,u64> rookMoves[64];//key, moves bitboard 
  std::map<u64,u64> bishopMoves[64];//key, moves bitboard 
  u64 rookMasks[64];
  u64 bishopMasks[64];
  
public:
  MagicMan();
  void searchForMagics();
  void saveMagics();
  void loadMagics();

  u64 rookLookup(u64 blockers, byte rookSquare);
  u64 bishopLookup(u64 blockers, byte bishopSquare);
};
