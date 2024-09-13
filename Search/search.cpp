#include "search.h"
Search::Search() {
  loadMagics();
  generateFileMasks();
  generateRankMasks();
  generateKnightMoves();
  generateRookMasks();
  generateBishopMasks();
  generateKingMoves();
  fillRookMoves();
  fillBishopMoves();
}
void Search::generateMoves(Board board, MoveList &moves) {
  friendlyBitboard = (board.flags & WHITE_TO_MOVE_BIT)
     ? board.bitboards[WHITE_PIECES]
     : board.bitboards[BLACK_PIECES];
  enemyBitboard = (board.flags & WHITE_TO_MOVE_BIT)
     ? board.bitboards[BLACK_PIECES]
     : board.bitboards[WHITE_PIECES];

  color = (board.flags & WHITE_TO_MOVE_BIT) ? WHITE : BLACK;
  moves.end = 0;
  addPawnMoves(board, moves);
  addSlidingMoves(board, moves);
  addKnightMoves(board, moves);
  addKingMoves(board, moves);
}

void Search::addSlidingMoves(Board board, MoveList &moves) {
  u64 horizontalPieces = board.bitboards[color + ROOK] | board.bitboards[color + QUEEN];
  u64 diagonalPieces = board.bitboards[color + BISHOP] | board.bitboards[color + QUEEN];
  while (horizontalPieces) {
    addHorizontalMoves(board, popls1b(horizontalPieces), moves);
  }
  while (diagonalPieces) {
    addDiagonalMoves(board, popls1b(diagonalPieces), moves);
  }
}

void Search::addPawnMoves(Board board, MoveList &moves) {
  int dir = board.flags & WHITE_TO_MOVE_BIT ? 1 : -1;
  // forward pawn moves
  u64 pawnDestinations = signedShift(board.bitboards[color + PAWN], 8 * dir);
  pawnDestinations &= ~board.occupancy;
  while (pawnDestinations) {
    Move move;
    move.to = popls1b(pawnDestinations);
    move.from = move.to - (8 * dir);
    moves.append(move);
  }
  // double forward moves
  pawnDestinations =
      board.bitboards[color + PAWN] &
      (board.flags & WHITE_TO_MOVE_BIT ? rankMasks[1] : rankMasks[6]);
  pawnDestinations = signedShift(pawnDestinations, 16 * dir);
  pawnDestinations &=  ~board.occupancy;
  while (pawnDestinations) {
    Move move;
    move.to = popls1b(pawnDestinations);
    move.from = move.to - (16 * dir);
    moves.append(move);
  }

  // pawn captures
  pawnDestinations = signedShift(board.bitboards[color + PAWN], 7 * dir);
  pawnDestinations &=
      (board.flags & WHITE_TO_MOVE_BIT) ? ~fileMasks[7] : ~fileMasks[0];
  pawnDestinations &= enemyBitboard;
  while (pawnDestinations) {
    Move move;
    move.to = popls1b(pawnDestinations);
    move.from = move.to - (7 * dir);
    moves.append(move);
  }

  pawnDestinations = signedShift(board.bitboards[color + PAWN], 9 * dir);
  pawnDestinations &=
      (board.flags & WHITE_TO_MOVE_BIT) ? ~fileMasks[0] : ~fileMasks[7];
  pawnDestinations &= enemyBitboard;
  while (pawnDestinations) {
    Move move;
    move.to = popls1b(pawnDestinations);
    move.from = move.to - (9 * dir);
    moves.append(move);
  }
}

void Search::addMovesToSquares(MoveList &moves, int fromSquare, u64 squares){
  while (squares) {
    Move move;
    move.to = popls1b(squares);
    move.from = fromSquare;
    moves.append(move);
  }
}
void Search::addHorizontalMoves(Board board, int square, MoveList &moves) {
  u64 blockers = board.occupancy & rookMasks[square];
  u64 hashed = (blockers * rookMagics[square]) >> rookShifts[square];
  u64 destinations = rookMoves[square][hashed] & (~friendlyBitboard);
  addMovesToSquares(moves, square, destinations);
};
void Search::addDiagonalMoves(Board board, int square, MoveList &moves) {
  u64 blockers = board.occupancy & bishopMasks[square];
  u64 hashed = (blockers * bishopMagics[square]) >> bishopShifts[square];
  u64 destinations = bishopMoves[square][hashed] & (~friendlyBitboard);
  addMovesToSquares(moves, square, destinations);
};

void Search::addKnightMoves(Board board, MoveList &moves) {
  u64 friendlyKnights = board.bitboards[color + KNIGHT];
  while (friendlyKnights) {
    int square = popls1b(friendlyKnights);
    u64 targets = knightMoves[square] & (~friendlyBitboard);
    addMovesToSquares(moves, square, targets);
  }
}

void Search::addKingMoves(Board board, MoveList &moves) {
  int square = popls1b(board.bitboards[color + KING]);
  u64 targets = kingMoves[square] & (~friendlyBitboard);
  addMovesToSquares(moves, square, targets);
}

// Generate Masks, Move Lookups, ect
//Only need to run once, but should still be understandable


void Search::generateRankMasks() {
  for (int i = 0; i < 8; i++) {
    rankMasks[i] = (u64)255 << (8 * i);
  }
}
void Search::generateFileMasks() {
  for (int i = 0; i < 8; i++) {
    u64 mask = (u64)0;
    for (int y = 0; y < 8; y++) 
      setBit(mask, (y * 8) + i);
    fileMasks[i] = mask;
  }
}

void Search::generateRookMasks() {
  for (int rank = 0; rank < 8; rank++) {   // y
    for (int file = 0; file < 8; file++) { // x
      u64 mask = (u64)0;
      mask = fileMasks[file] | rankMasks[rank];
      resetBit(mask, (rank * 8) + file);
      rookMasks[(rank * 8) + file] = mask;
    }
  }
}

void Search::generateBishopMasks() {
  int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  for (int rank = 0; rank < 8; rank++) {   // y
    for (int file = 0; file < 8; file++) { // x
      u64 mask = (u64)0;
      for (int i = 0; i < 4; i++) {
        int x = file;
        int y = rank;
        while ((x >= 0 && x < 8) && (y >= 0 && y < 8)) {
          setBit(mask, (y * 8) + x);
          x += directions[i][0];
          y += directions[i][1];
        }
      }
      resetBit(mask, (rank * 8) + file);
      bishopMasks[(rank * 8) + file] = mask;
    }
  }
}

void Search::generateKnightMoves() {
  int offsets[8][2] = {{1, -2}, {2, -1}, {2, 1},   {1, 2},
   {-1, 2}, {-2, 1}, {-2, -1}, {-1, -2}};
  for (int rank = 0; rank < 8; rank++) {   // y
    for (int file = 0; file < 8; file++) { // x
      u64 moves = (u64)0;
      for (int i = 0; i < 8; i++) {
        int x = file + offsets[i][0];
        int y = rank + offsets[i][1];
        if (x >= 0 && x < 8 && y >= 0 && y < 8) {
          int destination = (y * 8) + x;
          setBit(moves, destination);
        }
      }
      knightMoves[(rank * 8) + file] = moves;
    }
  }
}

void Search::generateKingMoves() {
  int offsets[8][2] = {{1, -1}, {-1, 1}, {-1, -1}, {1, 1},
   {1, 0},  {-1, 0}, {0, -1},  {0, 1}};
  for (int rank = 0; rank < 8; rank++) {   // y
    for (int file = 0; file < 8; file++) { // x
      u64 moves = (u64)0;
      for (int i = 0; i < 8; i++) {
        int x = file + offsets[i][0];
        int y = rank + offsets[i][1];
        if (x >= 0 && x < 8 && y >= 0 && y < 8) {
          int destination = (y * 8) + x;
          setBit(moves, destination);
        }
      }
      kingMoves[(rank * 8) + file] = moves;
    }
  }
}

void Search::fillRookMoves() {
  generateRookBlockers();
  for (int i = 0; i < 64; i++) {
    for (u64 blocker : rookBlockers[i]) {
      u64 moves = (u64)0;
      int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
      for (int direction = 0; direction < 4; direction++) {
        int x = i % 8;
        int y = floor(i / 8);
        while ((x >= 0 && x < 8) && (y >= 0 && y < 8)) {
          setBit(moves, (y * 8) + x);
          if (getBit(blocker, (y * 8) + x))
            break;
          x += directions[direction][0];
          y += directions[direction][1];
        }
      }
      rookMoves[i].insert({(blocker * rookMagics[i]) >> rookShifts[i], moves});
    }
  }
}

void Search::fillBishopMoves() {
  generateBishopBlockers();
  for (int i = 0; i < 64; i++) {
    for (u64 blocker : bishopBlockers[i]) {
      u64 moves = (u64)0;
      int directions[4][2] = {{-1, -1}, {1, 1}, {1, -1}, {-1, 1}};
      for (int direction = 0; direction < 4; direction++) {
        int x = i % 8;
        int y = floor(i / 8);
        while ((x >= 0 && x < 8) && (y >= 0 && y < 8)) {
          setBit(moves, (y * 8) + x);
          if (getBit(blocker, (y * 8) + x))
            break;
          x += directions[direction][0];
          y += directions[direction][1];
        }
      }
      bishopMoves[i].insert(
          {(blocker * bishopMagics[i]) >> bishopShifts[i], moves});
    }
  }
}


u64 Search::perftTest(Board &b, int depth){
  if(depth <= 0){return 1;}
  u64 count = 0;
  MoveList moves;
  generateMoves(b, moves);
  for(byte i = 0; i<moves.end;i++){
    b.makeMove(moves.moves[i]);
    count += perftTest(b, depth-1);
    b.unmakeMove(moves.moves[i]);
  }
  return count;
}
void Search::runMoveGenerationTest(){
  Board board;
  //https://www.chessprogramming.org/Perft_Results
  //position 6
  u64 expected[8] = {
  0,
  20,
  400,
  8902,
  197281,
  4865609,
  119060324,
  3195901860
  };
  board.loadFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  for(int i = 1; i<8; i++){
    u64 found = perftTest(board,i);
    if(found != expected[i]){
      std::cout<<"\x1b[31m";
    }else{
      std::cout<<"\x1b[32m";
    }
    std::cout<<"Depth: "<<i<<" Found: "<<found<<"/"<<expected[i]<<"\n";
  }
}