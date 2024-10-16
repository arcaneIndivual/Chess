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
void Search::generateMoves(Board &board, MoveList &moves) {
  if(!(board.flags&THREATENED_POPULATED)){
    board.flags |= THREATENED_POPULATED;
    generateMoves(board, moves);
    board.flags ^= WHITE_TO_MOVE_BIT;
    generateMoves(board, moves);
    board.flags ^= WHITE_TO_MOVE_BIT;
  }
  friendlyBitboard = (board.flags & WHITE_TO_MOVE_BIT)
     ? board.bitboards[WHITE_PIECES]
     : board.bitboards[BLACK_PIECES];
  enemyBitboard = (board.flags & WHITE_TO_MOVE_BIT)
     ? board.bitboards[BLACK_PIECES]
     : board.bitboards[WHITE_PIECES];

  color = (board.flags & WHITE_TO_MOVE_BIT) ? WHITE : BLACK;
  threatenedIndex = (board.flags & WHITE_TO_MOVE_BIT) ? 0 : 1;
  board.threatened[threatenedIndex] = (u64)0;
  moves.end = 0;
  addPawnMoves(board, moves);
  addSlidingMoves(board, moves);
  addKnightMoves(board, moves);
  addKingMoves(board, moves);
  addCastlingMoves(board, moves);
  if(!inFilter){
    filterLegalMoves(board, moves);
  }
}

void Search::filterLegalMoves(Board board, MoveList &moves){
  inFilter = true;
  byte friendlyColor = color;
  byte opponentColor = (color == WHITE)? BLACK : WHITE;
  for(int i = moves.end-1; i>=0; i--){
    board.makeMove(moves.moves[i]);
    byte kingSquare = bitScanForward(board.bitboards[friendlyColor+KING]);
    bool isLegal = !isAttacked(board, kingSquare, opponentColor);
    board.unmakeMove(moves.moves[i]);
    moves.moves[i].resetUnmakeData();
    if(!isLegal){
       moves.remove(i);
    }
  }
  inFilter = false;
}

bool Search::isAttacked(Board const &board, byte square, byte opponentColor){
  //attacked by knight
  u64 possibleKnights = knightMoves[square];
  if(possibleKnights&board.bitboards[KNIGHT+opponentColor]) return true;
  
  //attacked by king
  u64 possibleKings = kingMoves[square];
  if(possibleKings & board.bitboards[KING+opponentColor]) return true;
  //attacked by sliders
  u64 blockers = board.occupancy & rookMasks[square];
  u64 hashed = (blockers * rookMagics[square]) >> rookShifts[square];
  u64 possibleRooks = rookMoves[square][hashed];
  if(possibleRooks&board.bitboards[ROOK+opponentColor]) return true;

  blockers = board.occupancy & bishopMasks[square];
  hashed = (blockers * bishopMagics[square]) >> bishopShifts[square];
  u64 possibleBishops = bishopMoves[square][hashed];
  if(possibleBishops&board.bitboards[BISHOP+opponentColor]) return true;

  if((possibleBishops|possibleRooks)&board.bitboards[QUEEN+opponentColor]) return true;
  
  //attacked by pawn
  u64 possiblePawns = u64(0);
  if(opponentColor == WHITE){
    if(square%8 != 0)setBit(possiblePawns, square-9);
    if(square%8 != 7)setBit(possiblePawns, square-7);
  }else{
    if(square%8 != 7)setBit(possiblePawns, square+9);
    if(square%8 != 0)setBit(possiblePawns, square+7);
  }
  if(possiblePawns & board.bitboards[PAWN + opponentColor]) return true;
  
  return false;
}
void Search::addSlidingMoves(Board &board, MoveList &moves) {
  u64 horizontalPieces = board.bitboards[color + ROOK] | board.bitboards[color + QUEEN];
  u64 diagonalPieces = board.bitboards[color + BISHOP] | board.bitboards[color + QUEEN];
  while (horizontalPieces) {
    addHorizontalMoves(board, popls1b(horizontalPieces), moves);
  }
  while (diagonalPieces) {
    addDiagonalMoves(board, popls1b(diagonalPieces), moves);
  }
}

void Search::addMovesFromOffset(MoveList &moves, int offset, u64 targets, byte flags){
  while (targets) {
    byte to = popls1b(targets);
    if(to<8 || to>55){ 
      for(int i = BISHOP; i<= QUEEN;i++){
        Move move;
        move.setTo(to);
        move.setFrom(to + offset);
        move.setPromotion(i);
        moves.append(move);
      }
      continue;
    }
    Move move;
    move.setTo(to);
    move.setFrom(to + offset);
    move.setSpecialMoveData(flags);
    moves.append(move);
  }
}

void Search::addPawnMoves(Board &board, MoveList &moves) {
  int dir = board.flags & WHITE_TO_MOVE_BIT ? 1 : -1;
  u64 leftFileMask = (board.flags & WHITE_TO_MOVE_BIT) ? fileMasks[7] : fileMasks[0];
  u64 rightFileMask = (board.flags & WHITE_TO_MOVE_BIT) ? fileMasks[0] : fileMasks[7];
  u64 startRank = (board.flags & WHITE_TO_MOVE_BIT ? rankMasks[1] : rankMasks[6]);
  // forward pawn moves
  u64 pawnDestinations = signedShift(board.bitboards[color + PAWN], 8 * dir);
  pawnDestinations &= ~board.occupancy;
  addMovesFromOffset(moves, -8*dir, pawnDestinations);
  
  // double forward moves
  pawnDestinations = board.bitboards[color + PAWN] & startRank;
  pawnDestinations = signedShift(pawnDestinations, 8 * dir);
  pawnDestinations &=  ~board.occupancy;
  pawnDestinations = signedShift(pawnDestinations, 8 * dir);
  pawnDestinations &=  ~board.occupancy;
  addMovesFromOffset(moves, -16*dir, pawnDestinations);

  // pawn captures
  pawnDestinations = signedShift(board.bitboards[color + PAWN], 7 * dir);
  pawnDestinations &= ~leftFileMask & enemyBitboard;
  board.threatened[threatenedIndex] |= pawnDestinations;
  addMovesFromOffset(moves, -7*dir, pawnDestinations);

  pawnDestinations = signedShift(board.bitboards[color + PAWN], 9 * dir);
  pawnDestinations &= ~rightFileMask & enemyBitboard;
  board.threatened[threatenedIndex] |= pawnDestinations;
  addMovesFromOffset(moves, -9*dir, pawnDestinations);

  //En Passan
  if(board.enPassanTarget != EN_PASSAN_NULL){
    pawnDestinations = signedShift(board.bitboards[color + PAWN], 7 * dir);
    pawnDestinations &= ~leftFileMask;
    pawnDestinations &= (u64)1<<board.enPassanTarget;
    addMovesFromOffset(moves, -7*dir, pawnDestinations, EN_PASSAN);

    pawnDestinations = signedShift(board.bitboards[color + PAWN], 9 * dir);
    pawnDestinations &= ~rightFileMask;
    pawnDestinations &= (u64)1<<board.enPassanTarget;
    addMovesFromOffset(moves, -9*dir, pawnDestinations,EN_PASSAN);
  }
}

void Search::addMovesToSquares(MoveList &moves, int fromSquare, u64 squares){
  while (squares) {
    Move move;
    move.setTo(popls1b(squares));
    move.setFrom(fromSquare);
    moves.append(move);
  }
}
void Search::addHorizontalMoves(Board &board, int square, MoveList &moves) {
  u64 blockers = board.occupancy & rookMasks[square];
  u64 hashed = (blockers * rookMagics[square]) >> rookShifts[square];
  u64 destinations = rookMoves[square][hashed] & (~friendlyBitboard);
  board.threatened[threatenedIndex] |= destinations;
  addMovesToSquares(moves, square, destinations);
};

void Search::addDiagonalMoves(Board &board, int square, MoveList &moves) {
  u64 blockers = board.occupancy & bishopMasks[square];
  u64 hashed = (blockers * bishopMagics[square]) >> bishopShifts[square];
  u64 destinations = bishopMoves[square][hashed] & (~friendlyBitboard);
  board.threatened[threatenedIndex] |= destinations;
  addMovesToSquares(moves, square, destinations);
};

void Search::addKnightMoves(Board &board, MoveList &moves) {
  u64 friendlyKnights = board.bitboards[color + KNIGHT];
  while (friendlyKnights) {
    int square = popls1b(friendlyKnights);
    u64 targets = knightMoves[square] & (~friendlyBitboard);
    board.threatened[threatenedIndex] |= targets;
    addMovesToSquares(moves, square, targets);
  }
}

void Search::addKingMoves(Board &board, MoveList &moves) {
  int square = bitScanForward(board.bitboards[color + KING]);
  u64 targets = kingMoves[square] & (~friendlyBitboard);
  board.threatened[threatenedIndex] |= targets;
  addMovesToSquares(moves, square, targets);
}

void Search::addCastlingMoves(Board &board, MoveList &moves){
  byte opponentColor = (color == WHITE)? BLACK : WHITE;
  if(isAttacked(board,(byte)bitScanForward(board.bitboards[color+KING]),opponentColor)) return;//cannot castle out of check
  int mustBeEmpty[4][3] = {{2,2,1},{4,5,6},{57,58,58},{62,61,60}};
  int mustBeSafe [4][2] = {{2,1},{4,5},{57,58},{61,60}};
  byte masks[4] = {WHITE_KINGSIDE_BIT,WHITE_QUEENSIDE_BIT,BLACK_KINGSIDE_BIT,BLACK_QUEENSIDE_BIT};
  int i = (board.flags&WHITE_TO_MOVE_BIT)? 0 : 2;
  int max = i+2;
  for(int j = i; j<max; j++){
    bool legal = true; 
    for(int s : mustBeEmpty[j]){
      if(board.squares[s] != EMPTY){
        legal = false;
        break;
      }
    }
    if(!legal) continue;
    for(int s : mustBeSafe[j]){
      if(isAttacked(board, s, opponentColor)){
        legal = false;
        break;
      }
    }
    if(!legal) continue;
    if(board.flags&masks[j]){
      Move m;
      if(j%2 == 0){
        m.setSpecialMoveData(CASTLE_KINGSIDE); 
      }else{
        m.setSpecialMoveData(CASTLE_QUEENSIDE); 
      }
      moves.append(m);
    }
  }

}
//Generate Masks, Move Lookups, ect
//Only need to run once

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
        int y = i / 8;
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
        int y = i / 8;
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


u64 Search::perftTest(Board &b, int depth, bool root){
  /*if(!b.validate()) {
    debug::Settings s;
    std::cout<<"\x1b[31m[error] Invalid board, aborting branch [depth: "<<depth<<"]\x1b[0m\n"<<debug::printBoard(s,b)<<std::endl;
    return 0;
  }*/
  if(depth <= 0){return 1;}
  u64 count = 0;
  MoveList moves;
  generateMoves(b, moves);
  for(byte i = 0; i<moves.end;i++){
    b.makeMove(moves.moves[i]);
    u64 found = perftTest(b, depth-1,false);
    b.unmakeMove(moves.moves[i]);
    if(root){
      std::cout<<debug::moveToStr(moves.moves[i])<<" : "<<found<<std::endl;
    }
    count += found;
  }
  return count;
}

void Search::runMoveGenerationTest(Board &board){
  //https://www.chessprogramming.org/Perft_Results
  debug::Settings settings;
  for(int i = 1; i<5; i++){
    std::cout<<"\x1b[0mDepth: "<<i<<"\x1b[30m \n";
    u64 found = perftTest(board,i);
    if(found == 0) return;
    std::cout<<"\x1b[0mFound: "<<found<<"\n"<<std::endl;
  }
}

void Search::runMoveGenerationSuite(){
  Board board;
  //https://www.chessprogramming.org/Perft_Results
  std::string positions[8] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 ",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8  ",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ",
    "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1 ",
    "8/3K4/2p5/p2b2r1/5k2/8/8/1q6 b - 1 67"
  };
  u64 expected[8] = {
    4865609,
    4085603,
    674624,
    422333,
    2103487,
    3894594,
    3605103,
    279
  };
  int depths[8] = {
    5,
    4,
    5,
    4,
    4,
    4,
    5,
    2
  };
  std::cout<<"Starting"<<std::endl;
  debug::Settings settings;
  u64 sum = 0;
  auto start = std::chrono::high_resolution_clock::now();
  for(int i = 0; i<8; i++){
    board.loadFromFEN(positions[i]);
    u64 found = perftTest(board,depths[i],false);
    sum += found;
    std::cout<<"Depth: "<<depths[i];
    std::cout<<" Found: ";
    if(found != expected[i]){
      std::cout<<"\x1b[31m";
    }else{
      std::cout<<"\x1b[32m";
    }
    std::cout<<found<<"/"<<expected[i]<<"\x1b[0m"<<std::endl;
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = end-start;
  std::cout<<"Searched "<< sum << " moves\n";
  std::cout<<"Finished in "<<(float)std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()/1000.f<<"s"<<std::endl;
}