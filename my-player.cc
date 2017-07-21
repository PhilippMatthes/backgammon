#include <stdio.h>
#include <state.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <iostream>
#include <mcp.h>
#include <state.h>
#include <vector>
#include <algorithm>


// Forward declarations
typedef std::vector<game_move> gm_vector;
typedef std::vector<int> int_vector;
typedef typename std::vector<game_move>::const_iterator gm_iterator;
typedef typename std::vector<int>::const_iterator int_iterator;

int moves_left (game_state * const state);
int modulo(int x, int N);
int get_bar_amount(game_state * const state);
int post_normal_moves(game_state * const state, game_move move, int dice);
int post_bear_moves(game_state * const state, game_move move, int dice);
int post_blot_moves(game_state * const state, game_move move, int dice);
int post_save_positions(game_state * const state, game_move move, int dice);
int post_blot_positions(game_state * const state, game_move move, int dice);
int back_to_forth_factor(game_state * const state, game_move move);
int target_blots_factor(game_state * const state, game_move move, int dice);
int post_risk_of_getting_hit(game_state * const state, game_move move, int dice);
int evaluator(game_state * const state, game_move move);
int select_dice(int_vector used_dice);
int get_init_bear_off_position(game_state * const state);

bool ready_to_bear_off (game_state * const state);
bool enemies_blot (game_state * const state, int target);
bool only_one_checker_left(game_state * const state);

int_vector get_usable_positions(game_state * const state);
int_vector get_locked_positions(game_state * const state);

game_move move_with_highest_evaluation(game_state * const state, gm_vector moves);
game_move get_bar_move(game_state * const state, int_vector used_dice);
game_move get_normal_move(game_state * const state, int_vector used_dice);
game_move get_fitting_bear_move(game_state * const state, int_vector used_dice);
game_move get_bear_move(game_state * const state, int_vector used_dice);

gm_vector select_moves(game_state * const state);
gm_vector get_possible_normal_moves(game_state * const state, int_vector used_dice);
gm_vector get_combined_move(game_state * const state);

// Main block
int main(int, char**) {

  // Initialize random generator
  // printf("%d: \n",__LINE__);
  time_t t;
  time(&t);
  srand((unsigned int)t);

  while(1){

    game_state state1;
    game_state state2;
    multi_move mmove;

    while (1) {

      // Fetch state
      // printf("%d: Fetch state\n",__LINE__);
      initialize_multi_move(&mmove);
      if (! deserialize_state(CHILD_IN_FD, &state1) ) { abort(); }
      state2 = state1;
      print_state(&state1);

      // Select moves
      int_vector used_dice;
      // printf("%d: Select moves\n",__LINE__);
      gm_vector selected_moves = select_moves(&state1);
      // for (game_move m : selected_moves)
        // printf("Normally selected moves: (%d,%d)\n",m.point_from,m.roll);
      gm_vector alternative_moves = get_combined_move(&state2);
      // for (game_move m : alternative_moves)
        // printf("Alternatively selected moves: (%d,%d)\n",m.point_from,m.roll);
      if ((int)alternative_moves.size() > (int)selected_moves.size())
        selected_moves = alternative_moves;

      // Apply moves
      // printf("%d: Apply moves\n",__LINE__);
      int c = 0;
      for (game_move i : selected_moves) {
        mmove.moves[c] = i;
        c++;
      }
      mmove.num_moves = selected_moves.size();

      // Output moves
      // printf("%d: Output moves\n",__LINE__);
      if (! serialize_moves(CHILD_OUT_FD, &mmove) ) { abort(); }

    }
  }
  return 0;
}

// Calculates available moves (2, double -> 4)
int moves_left (game_state * const state) {
  return (state->dice[0] != state->dice[1] ? NUM_DICE : 2 * NUM_DICE);
}

// Generate positive modulo numbers. Just % doesn't guarantee positive output
int modulo(int x, int N) {
  return (x % N + N) %N;
}

// Get amount of checkers on own bar
int get_bar_amount(game_state * const state) {
  if (state->player == 1)
    return get_lower_bar(state->board[POS_BAR]);
  else if (state->player == -1)
    return get_higher_bar(state->board[POS_BAR]);
  else {
    printf("%d: Error in get_bar_amount. Abort.\n",__LINE__);
    abort();
  }
}

// Check if we are ready to bear off
bool ready_to_bear_off (game_state * const state) {
  // Search for players checkers and return false
  // if checker is localized out of his home board.
  // Otherwise return true.
  if (state->player == -1) {
    for (int i = 1; i <= POINTS - HOME_POINTS; ++i){
      if ((state->player * state->board[i] > 0) or (get_higher_bar(state->board[POS_BAR]) > 0))
        return false;
    }
  }
  else if (state->player == 1) {
    for (int i = POINTS; i > HOME_POINTS; --i) {
      if ((state->player * state->board[i] > 0) or (get_lower_bar(state->board[POS_BAR]) > 0))
        return false;
    }
  }
  // printf("%d: Function ready_to_bear_off responded: ready to bear off now.\n",__LINE__);
  return true;
}

// Check if target contains enemies blot. Used for internal
// calculations to rebuild the board internally.
bool enemies_blot (game_state * const state, int target) {
  if (target > 0 and target < 25) {
    if (state->board[target] * (-1) * state->player == 1)
      return true;
  }
  return false;
}

// Select dice function. Responds random dice, if no dice has been used.
// Otherwise: Responds other dice.
int select_dice(int_vector used_dice) {
  if (used_dice.size() == 0)
    return rand() % 2;
  else if (used_dice.size() == 1) {
    if (used_dice[0] == 1)
      return 0;
    else
      return 1;
  }
  else
    return 0;
}

// Get all positions on which we have our checkers
int_vector get_usable_positions(game_state * const state) {
  int_vector output;
    for (int i = 1; i < POINTS + 1; ++i) {
      if (state->player * state->board[i] > 0)
        output.push_back(i);
      }
  return output;
}

// Gets the initial bear off position
int get_init_bear_off_position(game_state * const state) {
  if (state->player == -1) {
    for (int i = 19; i <= POINTS; ++i) {
      if (state->board[i] < 0) {
        // printf("%d: Init bear off position: %d\n",__LINE__,i);
        return i;
      }
    }
  }
  if (state->player == 1) {
    for (int i = HOME_POINTS; i > 0; --i) {
      if (state->board[i] > 0) {
        // printf("%d: Init bear off position: %d\n",__LINE__,i);
        return i;
      }
    }
  }
  return 0;
}

// Get all positions on which the enemy has his checkers
int_vector get_locked_positions(game_state * const state) {
  int_vector output;
    for (int i = 1; i <= POINTS + 1; i++) {
      if ((-1) * state->player * state->board[i] > 1)
        output.push_back(i);
      }
  return output;
}

// Get list of all possible moves (without bearing off -> "normal")
gm_vector get_possible_normal_moves(game_state * const state, int_vector used_dice) {
  gm_vector output;
  int_vector locked = get_locked_positions(state);
  int_vector usable = get_usable_positions(state);

  // Check all usable points and if they project to a locked target point.
  // If they don't, add them to the output.
  // For the first dice:
  int target = 25;
  for (int u : usable) {
    int dice = select_dice(used_dice);
    if (used_dice.size() == 0)
      used_dice.push_back(dice);
    bool is_locked = false;
    for (int l : locked) {
      target = u + (-1) * state->player * state->dice[dice];
      if (target == l)
        is_locked = true;
    }
    if (!is_locked and target > 0 and target < 25) {
      game_move move;
      move.point_from = u;
      move.roll = state->dice[dice];
      output.push_back(move);
    }
    // For the second dice:
    if ((int)used_dice.size() < moves_left(state)) {
      is_locked = false;
      dice = select_dice(used_dice);
      for (int l : locked) {
        target = u + (-1) * state->player * state->dice[dice];
        if (target == l)
          is_locked = true;
      }
      if (!is_locked and target > 0 and target < 25) {
        game_move move;
        move.point_from = u;
        move.roll = state->dice[dice];
        output.push_back(move);
      }
    }
  }
  return output;
}

// Get number of possible moves after a chosen move
int post_normal_moves(game_state * const state, game_move move, int dice) {
  game_state local = *state;
  int target;
  if (move.point_from != 0)
    target = move.point_from + (-1) * state->player * state->dice[dice];
  else
    target = modulo(move.point_from + (-1) * state->player * state->dice[dice],25);

  if (target > 24 or target < 1)
    return 0;
  // Apply move internally
  if (enemies_blot(state, target))
    local.board[target] = local.player * ((local.player * local.board[target]) + 2);
  else
    local.board[target] = local.player * ((local.player * local.board[target]) + 1);
  local.board[move.point_from] = local.player * ((local.player * local.board[move.point_from]) - 1);
  int_vector used_dice;
  used_dice.push_back(dice);
  // Get number of possible moves with internally applied changes
  gm_vector possible_moves = get_possible_normal_moves(&local, used_dice);
  // for (game_move m : possible_moves)
    // printf("%d: Possible move after chosen move (%d,%d): (%d,%d)\n",__LINE__,move.point_from,move.roll,m.point_from,m.roll);
  int number = (int)possible_moves.size();
  return number;
}

// Count all available bear off moves after move
int post_bear_moves(game_state * const state, game_move move, int dice) {
  game_state local = *state;
  // Apply move internally
  int target;
  if (move.point_from != 0)
    target = move.point_from + (-1) * state->player * state->dice[dice];
  else
    target = modulo(move.point_from + (-1) * state->player * state->dice[dice],25);
  if (target < 25 and target > 0) {
    if (enemies_blot(state, target))
      local.board[target] = local.player * ((local.player * local.board[target]) + 2);
    else
      local.board[target] = local.player * ((local.player * local.board[target]) + 1);
  }
  local.board[move.point_from] = local.player * ((local.player * local.board[move.point_from]) - 1);
  int_vector used_dice;
  used_dice.push_back(dice);

  if (! ready_to_bear_off(&local))
    return 0;

  // Get number of possible moves with internally applied changes
  game_move m = get_bear_move(&local, used_dice);
  if (m.roll == 0 and m.point_from == 0)
    m = get_fitting_bear_move(&local, used_dice);
  // printf("%d: Possible bear off move after chosen move (%d,%d): (%d,%d)\n",__LINE__,move.point_from,move.roll,m.point_from,m.roll);
  if (m.point_from != 0 and m.roll != 0) {
    // printf("Bear-off-able.\n");
    return 1;
  }
  return 0;
}

// Count all available blot moves after move
int post_blot_moves(game_state * const state, game_move move, int dice) {
  game_state local = *state;
  int target;
  if (move.point_from != 0)
    target = move.point_from + (-1) * state->player * state->dice[dice];
  else
    target = modulo(move.point_from + (-1) * state->player * state->dice[dice],25);

  if (target > 24 or target < 1)
    return 0;
  // Apply move internally
  if (enemies_blot(state, target))
    local.board[target] = local.player * ((local.player * local.board[target]) + 2);
  else
    local.board[target] = local.player * ((local.player * local.board[target]) + 1);
  local.board[move.point_from] = local.player * ((local.player * local.board[move.point_from]) - 1);
  int_vector used_dice;
  used_dice.push_back(dice);
  // Get possible moves with internally applied changes
  gm_vector possible_moves = get_possible_normal_moves(&local, used_dice);

  int number = 0;
  for (game_move m : possible_moves) {
    target = m.point_from + (-1) * state->player * state->dice[dice];
    if (enemies_blot(&local, target))
      number = number + 1;
  }
  // printf("%d: Blotable positions: %d\n",__LINE__,number);
  return number;
}

// Count all saved positions after move
int post_save_positions(game_state * const state, game_move move, int dice) {
  game_state local = *state;

  int target;
  if (move.point_from != 0)
    target = move.point_from + (-1) * state->player * state->dice[dice];
  else
    target = modulo(move.point_from + (-1) * state->player * state->dice[dice],25);

  if (target > 24 or target < 1)
    return 0;
  // Apply move internally
  if (enemies_blot(state, target))
    local.board[target] = local.player * ((local.player * local.board[target]) + 2);
  else
    local.board[target] = local.player * ((local.player * local.board[target]) + 1);
  local.board[move.point_from] = local.player * ((local.player * local.board[move.point_from]) - 1);
  int_vector usable = get_usable_positions(&local);
  int number = 0;
  for (int u : usable) {
    if ((state->board[u] * state->player) > 1)
      number = number + 1;
  }
  // printf("%d: Save positions: %d\n",__LINE__,number);
  return number;
}

// Count all own blot positions after move
int post_blot_positions(game_state * const state, game_move move, int dice) {
  game_state local = *state;
  int target;
  if (move.point_from != 0)
    target = move.point_from + (-1) * state->player * state->dice[dice];
  else
    target = modulo(move.point_from + (-1) * state->player * state->dice[dice],25);

  if (target > 24 or target < 1)
    return 0;
  // Apply move internally
  if (enemies_blot(state, target))
    local.board[target] = local.player * ((local.player * local.board[target]) + 2);
  else
    local.board[target] = local.player * ((local.player * local.board[target]) + 1);
  local.board[move.point_from] = local.player * ((local.player * local.board[move.point_from]) - 1);
  int_vector usable = get_usable_positions(&local);
  int number = 0;
  for (int u : usable) {
    if ((state->board[u] * state->player) == 1)
      number = number + 1;
  }
  // printf("%d: Own blot positions: %d\n",__LINE__,number);
  return number;
}

// Select outer checkers rather than inner checkers
int back_to_forth_factor(game_state * const state, game_move move) {
  return modulo(move.point_from * state->player,25);
}

// Factor for aiming on blots
int target_blots_factor(game_state * const state, game_move move, int dice) {
  int_vector locked = get_locked_positions(state);
  int target;
  if (move.point_from != 0)
    target = move.point_from + (-1) * state->player * state->dice[dice];
  else
    target = modulo(move.point_from + (-1) * state->player * state->dice[dice],25);
  if (target > 24 or target < 1)
    return 0;
  for (int l : locked) {
    if ((target == l) and ((state->board[l] * (-1) * state->player) == 1))
      return 1;
  }
  return 0;
}

int post_risk_of_getting_hit(game_state * const state, game_move move, int dice) {
  game_state local = *state;
  int target;
  if (move.point_from != 0)
    target = move.point_from + (-1) * state->player * state->dice[dice];
  else
    target = modulo(move.point_from + (-1) * state->player * state->dice[dice],25);
  if (target > 24 or target < 1)
    return 0;
  // Apply move internally
  if (enemies_blot(state, target))
    local.board[target] = local.player * ((local.player * local.board[target]) + 2);
  else
    local.board[target] = local.player * ((local.player * local.board[target]) + 1);
  local.board[move.point_from] = local.player * ((local.player * local.board[move.point_from]) - 1);

  int number = 0;
  //  Calculate risk of getting hit
  int_vector usable = get_usable_positions(&local);
  int_vector blots;
  int_vector locked = get_locked_positions(&local);

  for (int u : usable) {
    if ((state->board[u] * state->player) == 1)
      blots.push_back(u);
  }
  if ((int)blots.size() == 0)
    return 0;
  for (int l : locked) {
    for (int b : blots) {
      target = l + state->player * 6;
      if (target > 0 and target < 25) {
        if ((state->player == 1) and (l > b and target < b))
          number = number + 1;
        if ((state->player == 1) and (l < b and target > b))
          number = number + 1;
      }
    }
  }
  return number;
}

// This is where the magic (heuristics evaluation) happens
int evaluator(game_state * const state, game_move move) {
  int dice;
  if (move.roll == state->dice[0])
    dice = 0;
  else
    dice = 1;
  if (post_normal_moves(state,move,dice) + post_bear_moves(state,move,dice) == 0)
    return -10000;
  int e =   1 *  post_bear_moves(state,move,dice)
          + 2 *  post_normal_moves(state,move,dice) // Careful: too high values result in 2nd World War
          + 5  *  post_blot_moves(state,move,dice)
          + 10  *  post_save_positions(state,move,dice)
          + 7  *  back_to_forth_factor(state,move)
          + 5 *  target_blots_factor(state,move,dice)
          - 9 *  post_risk_of_getting_hit(state,move,dice)
          - 5  *  post_blot_positions(state,move,dice);
  // printf("Evaluation for (%d,%d): %d\n",move.point_from,move.roll,e);
  return e;
}

// Get move with highest possible moves after this move
game_move move_with_highest_evaluation(game_state * const state, gm_vector moves) {

  game_move output;
  output = moves[0];
  // printf("Evaluator for (%d,%d): %d\n",moves[0].point_from,moves[0].roll,evaluator(state, moves[0]));
  // printf("Evaluator for (%d,%d): %d\n",moves[0].point_from,moves[0].roll,evaluator(state, moves[0]));


  if ((int)moves.size() == 2) {
    if ((evaluator(state, moves[0]) > -5000) and (evaluator(state, moves[1]) > -5000)) {
      if (moves[0].roll != moves[1].roll) {
        if (moves[0].roll > moves[1].roll)
          return moves[0];
        else
          return moves[1];
      }
    }
    if ((evaluator(state, moves[0]) < -5000) and (evaluator(state, moves[1]) < -5000)) {
      if (moves[0].roll != moves[1].roll) {
        if (moves[0].roll > moves[1].roll)
          return moves[0];
        else
          return moves[1];
      }
    }
  }

  // Check through all moves and update move if it has higher possibilities.
  for (game_move m : moves) {
    if (evaluator(state,m) > evaluator(state,output)) {
      output = m;
    }
  }
  // printf("%d: Move with highest evaluation (%d) = (%d,%d)\n",__LINE__,evaluator(state,output),output.point_from,output.roll);
  return output;
}

// Search move from bar
game_move get_bar_move(game_state * const state, int_vector used_dice) {
  game_move output;
  gm_vector found_moves;


  // Select dice and add it to the used dices (internally)
  int dice = select_dice(used_dice);
  used_dice.push_back(dice);

  // Get all locked positions and set target
  int_vector locked_list = get_locked_positions(state);
  int target = modulo(0 + (-1) * state->player * state->dice[dice], 25);

  bool is_locked = false;
  for (int i : locked_list) {
    if (i == target) {
      is_locked = true;
    }
  }
  if (! is_locked) {
    output.point_from = 0;
    output.roll = state->dice[dice];
    found_moves.push_back(output);
  }

  if ((int)used_dice.size() < moves_left(state)) {
    is_locked = false;
    dice = select_dice(used_dice);
    target = modulo(0 + (-1) * state->player * state->dice[dice], 25);
    for (int i : locked_list) {
      if (i == target) {
        is_locked = true;
      }
    }
    if (! is_locked) {
      output.point_from = 0;
      output.roll = state->dice[dice];
      found_moves.push_back(output);
    }
  }
  if ((int)found_moves.size() == 0){
    output.point_from = 0;
    output.roll = 0;
    return output;
  }
  // for (game_move m : found_moves) {
    // printf("%d: Found move from bar: (%d,%d)\n",__LINE__,m.point_from,m.roll);
  // }
  return move_with_highest_evaluation(state, found_moves);

}

// Search normal move
game_move get_normal_move(game_state * const state, int_vector used_dice) {
  // printf("%d: Search normal move\n",__LINE__);
  game_move output;
  gm_vector found_moves;

  // Select dice and add it to the used dices (internal)
  int dice = select_dice(used_dice);
  used_dice.push_back(dice);

  // Get all locked and usable positions
  int_vector locked_list = get_locked_positions(state);
  int_vector usable_list = get_usable_positions(state);


  // Get all possible moves (checking for bearing off) and select one.
  // printf("%d: Get all possible moves checking for bearing off and select one.\n",__LINE__);
  for (int i : usable_list) {
    bool is_locked = false;
    int target = i + (-1) * state->player * state->dice[dice];
    for (int c : locked_list) {
      if (c == target) {
        is_locked = true;
      }
    }
    if ((! is_locked) and (!( target > 24 or target < 1))) {
      // printf("%d: Add (%d,%d)->%d\n",__LINE__,i,state->dice[dice],target);
      game_move add;
      add.point_from = i;
      add.roll = state->dice[dice];
      found_moves.push_back(add);
    }
  }
  dice = select_dice(used_dice);
  for (int i : usable_list) {
    if((int)used_dice.size() < moves_left(state)) {
      bool is_locked = false;
      int target =  i + (-1) * state->player * state->dice[dice];
      for (int c : locked_list) {
        if (c == target)
          is_locked = true;
      }
      if ((! is_locked) and (!( target > 24 or target < 1))) {
        // printf("%d: Add (%d,%d)->%d\n",__LINE__,i,state->dice[dice],target);
        game_move add;
        add.point_from = i;
        add.roll = state->dice[dice];
        found_moves.push_back(add);
      }
    }
  }
  // for (game_move m: found_moves)
    // printf("Normal moves found: (%d,%d)\n",m.point_from,m.roll);
  if ((int)found_moves.size() == 0) {
    // printf("%d: Return (0,0)\n",__LINE__);
    output.point_from = 0;
    output.roll = 0;
    return output;
  }
  else {
    return move_with_highest_evaluation(state, found_moves);
  }
}

// Outputs a fitting bear move
game_move get_fitting_bear_move(game_state * const state, int_vector used_dice) {
  game_move output;
  int_vector usable = get_usable_positions(state);
  int target;


  int dice = select_dice(used_dice);
  used_dice.push_back(dice);

  for (int i : usable) {
    target = i + (-1) * state->player * state->dice[dice];
    if (target % 25 == 0) {
      output.point_from = i;
      output.roll = state->dice[dice];
      return output;
      }
  }

  if((int)used_dice.size() < moves_left(state)) {
    dice = select_dice(used_dice);

    for (int i : usable) {
      target = i + (-1) * state->player * state->dice[dice];
      if (target % 25 == 0) {
        output.point_from = i;
        output.roll = state->dice[dice];
        return output;
        }
    }
  }
  output.point_from = 0;
  output.roll = 0;
  return output;
}

// Search bear off move
game_move get_bear_move(game_state * const state, int_vector used_dice) {
  game_move output;
  gm_vector output_vector;
  int dice;

  if (used_dice.size() == 0 && (int)get_usable_positions(state).size() > 0) {
    if (state->dice[0] >= state->dice[1])
      dice = 0;
    else
      dice = 1;
    used_dice.push_back(dice);
    int init_bear_off_position = get_init_bear_off_position(state);
    // printf("%d: Bear off position: %d\n",__LINE__,init_bear_off_position);
    int target = init_bear_off_position + (-1) * state->player * state->dice[dice];
    // printf("%d: Target is: %d\n",__LINE__,target);
    if ((target > 24 or target < 1) && (init_bear_off_position != 0)) {
      output.point_from = init_bear_off_position;
      output.roll = state->dice[dice];
      output_vector.push_back(output);
    }
  }
  if (true) { // changed from: if((int)get_usable_positions(state).size() > 0) {
    dice = select_dice(used_dice);
    int init_bear_off_position = get_init_bear_off_position(state);
    int target = init_bear_off_position + (-1) * state->player * state->dice[dice];
    // printf("%d: Target is: %d\n",__LINE__,target);
    if ((target > 24 or target < 1) && (init_bear_off_position != 0)) {
      output.point_from = init_bear_off_position;
      output.roll = state->dice[dice];
      output_vector.push_back(output);
    }
  }
  if ((int)output_vector.size() == 2)
    return output_vector[0];
  if ((int)output_vector.size() == 1)
    return output;
  if ((int)output_vector.size() == 0) {
    output.point_from = 0;
    output.roll = 0;
  }
  return output;
}

// Is only one checker left?
bool only_one_checker_left(game_state * const state) {
  int_vector usable = get_usable_positions(state);
  // for (int i : usable) {
    // printf("Usable: %d\n",i);
    // printf("Board tells: %d\n",state->board[i]);
    // printf("Usable.size() = %d\n",(int)usable.size());
  // }
  if ((int)usable.size() == 1) {
    if (state->board[usable[0]] * state->player == 1)
      return true;
  }
  return false;
}

// Get combined move if the standard way returns a too short output
gm_vector get_combined_move(game_state * const state) {
  // Calc max moves
  int max_moves = moves_left(state);


  // Init list of chosen dice and output
  gm_vector output;
  output.clear();
  int_vector chosen_dice;
  chosen_dice.clear();



  // Loopback
  while (max_moves > 0 && ((int)get_usable_positions(state).size() + get_bar_amount(state)) > 0) {
    if (get_bar_amount(state) > 0) {

      // Search for move from bar
      game_move bar_move = get_bar_move(state, chosen_dice);

      // Add chosen dice
      // printf("%d: Add chosen dice\n",__LINE__);
      if (state->dice[0] == bar_move.roll)
        chosen_dice.push_back(0);
      else if (state->dice[1] == bar_move.roll)
        chosen_dice.push_back(1);
      else if (! (0 == bar_move.roll)) {
        printf("%d: Error in add dice: dice did not match roll.\n",__LINE__);
        abort();
      }

      // Returned move not empty?
      if (!(bar_move.point_from == 0 and bar_move.roll == 0)) {

        // Apply move to state
        // printf("%d: Apply move to state\n",__LINE__);
        int target = modulo(bar_move.point_from + (-1) * state->player * bar_move.roll, 25);
        if (target <= 0) {
          printf("%d: Error in modulo. Target is %d.\n",__LINE__,target);
          abort();
        }
        if (enemies_blot(state, target))
          state->board[target] = state->player * ((state->player * state->board[target]) + 2);
        else
          state->board[target] = state->player * ((state->player * state->board[target]) + 1);
        if (state->player == 1)
          set_lower_bar(&state->board[0], get_lower_bar(state->board[0]) - 1);
        else if (state->player == -1)
          set_higher_bar(&state->board[0], get_higher_bar(state->board[0]) - 1);
        else {
          printf("%d: Abort due to state->player != -1 and != 1\n",__LINE__);
          abort();
        }

        // Add move to output
        output.push_back(bar_move);

      }
      else {
        printf("%d: There is a checker on the bar but no move was found.\n",__LINE__);
      }

      // Decrement max moves
      max_moves = max_moves - 1;

    }
    else {
    if (get_usable_positions(state).size() > 0) {

      game_move move;
      // ----------------------------NORMAL-----------------------------
      // Search for move but don't bear off
      move = get_normal_move(state, chosen_dice);
      // printf("Get normal move resulted: (%d,%d)\n",move.point_from,move.roll);
      // Add chosen dice
      if (state->dice[0] == move.roll)
        chosen_dice.push_back(0);
      else if (state->dice[1] == move.roll)
        chosen_dice.push_back(1);
      else if (! (0 == move.roll)) {
        printf("%d: Error in add dice: dice did not match roll. Revieved roll: %d\n",__LINE__,move.roll);
        abort();
      }
      // Returned move empty?
      if (!(move.point_from == 0 and move.roll == 0)) {
        // Apply move to state
        int target = modulo(move.point_from + (-1) * state->player * move.roll, 25);
        if (target <= 0) {
          printf("%d: Error in modulo. Target is %d.\n",__LINE__,target);
          abort();
        }
        if (enemies_blot(state, target))
          state->board[target] = state->player * ((state->player * state->board[target]) + 2);
        else
          state->board[target] = state->player * ((state->player * state->board[target]) + 1);
        state->board[move.point_from] = ((state->player * state->board[move.point_from]) - 1) * state->player;
        // Add move to output
        output.push_back(move);
      }
      // ----------------------------NORMAL-----------------------------

        else if (ready_to_bear_off(state) && (int)chosen_dice.size() < moves_left(state)){
          // ----------------------------FITTING-----------------------------
          move = get_fitting_bear_move(state, chosen_dice);
          if (!(move.point_from == 0 and move.roll == 0) and (!only_one_checker_left(state))) {
            // Apply move to state
            state->board[move.point_from] = state->player * ((state->player * state->board[move.point_from]) - 1);
            // Add chosen dice
            if (state->dice[0] == move.roll)
              chosen_dice.push_back(0);
            else if (state->dice[1] == move.roll)
              chosen_dice.push_back(1);
            else if (! (0 == move.roll)){
              printf("%d: Error in add dice: dice did not match roll.\n",__LINE__);
              abort();
            }
              // Add move to output
              output.push_back(move);
            }
            // ----------------------------FITTING-----------------------------
          else {
            // ----------------------------BEAR-----------------------------
            // Search for bear off move
            move = get_bear_move(state, chosen_dice);
            // printf("%d: Bear move is (%d,%d)\n",__LINE__,move.point_from,move.roll);
            // Returned move empty?
            if (!(move.point_from == 0 and move.roll == 0)) {
              // Add chosen dice
              if (state->dice[0] == move.roll)
                chosen_dice.push_back(0);
              else if (state->dice[1] == move.roll)
                chosen_dice.push_back(1);
              else if (! (0 == move.roll)){
                printf("%d: Error in add dice: dice did not match roll.\n",__LINE__);
                abort();
              }
              // Apply move to state
              state->board[move.point_from] = state->player * ((state->player * state->board[move.point_from]) - 1);
              // Add move to output
              output.push_back(move);
            }
            // ----------------------------BEAR-----------------------------

          }
        }
        // Decrement max moves
        max_moves = max_moves - 1;
      }
      else {

        // Search for move but don't bear off
        game_move move = get_normal_move(state, chosen_dice);

        // Add chosen dice
        if (state->dice[0] == move.roll)
          chosen_dice.push_back(0);
        else if (state->dice[1] == move.roll)
          chosen_dice.push_back(1);
        else if (! (0 == move.roll)) {
          printf("%d: Error in add dice: dice did not match roll. Revieved roll: %d\n",__LINE__,move.roll);
          abort();
        }

        // Returned move empty?
        if (!(move.point_from == 0 and move.roll == 0)) {

          // Apply move to state
          int target = modulo(move.point_from + (-1) * state->player * move.roll, 25);
          if (target <= 0) {
            printf("%d: Error in modulo. Target is %d.\n",__LINE__,target);
            abort();
          }
          if (enemies_blot(state, target))
            state->board[target] = state->player * ((state->player * state->board[target]) + 2);
          else
            state->board[target] = state->player * ((state->player * state->board[target]) + 1);

          state->board[move.point_from] = ((state->player * state->board[move.point_from]) - 1) * state->player;

          // Add move to output
          output.push_back(move);

        }
        // Decrement max moves
        max_moves = max_moves - 1;
      }
    }
  }
  return output;
}

// Mother of all functions. Selects the moves.
gm_vector select_moves(game_state * const state) {

  // Calc max moves
  int max_moves = moves_left(state);

  // Init list of chosen dice and output
  gm_vector output;
  output.clear();
  int_vector chosen_dice;
  chosen_dice.clear();

  // Loopback
  while (max_moves > 0 && ((int)get_usable_positions(state).size() + get_bar_amount(state)) > 0) {
    if (get_bar_amount(state) > 0) {

      // Search for move from bar
      // printf("%d: Search for move from bar\n",__LINE__);
      game_move bar_move = get_bar_move(state, chosen_dice);

      // Add chosen dice
      // printf("%d: Add chosen dice\n",__LINE__);
      if (state->dice[0] == bar_move.roll)
        chosen_dice.push_back(0);
      else if (state->dice[1] == bar_move.roll)
        chosen_dice.push_back(1);
      else if (! (0 == bar_move.roll)) {
        printf("%d: Error in add dice: dice did not match roll.\n",__LINE__);
        abort();
      }

      // Returned move not empty?
      if (!(bar_move.point_from == 0 and bar_move.roll == 0)) {

        // Apply move to state
        // printf("%d: Apply move to state\n",__LINE__);
        int target = modulo(bar_move.point_from + (-1) * state->player * bar_move.roll, 25);
        if (target <= 0) {
          printf("%d: Error in modulo. Target is %d.\n",__LINE__,target);
          abort();
        }
        if (enemies_blot(state, target))
          state->board[target] = state->player * ((state->player * state->board[target]) + 2);
        else
          state->board[target] = state->player * ((state->player * state->board[target]) + 1);
        if (state->player == 1)
          // Decrement lower bar
          set_lower_bar(&state->board[0], get_lower_bar(state->board[0]) - 1);
        else if (state->player == -1)
          // Decrement higher bar
          set_higher_bar(&state->board[0], get_higher_bar(state->board[0]) - 1);
        else {
          printf("%d: Abort due to state->player != -1 and != 1\n",__LINE__);
          abort();
        }

        // Add move to output
        output.push_back(bar_move);

      }
      else {
        printf("%d: There is a checker on the bar but no move was found.\n",__LINE__);
      }

      // Decrement max moves
      max_moves = max_moves - 1;

    }
    else {
    if (ready_to_bear_off(state) && get_usable_positions(state).size() > 0) {
      // printf("%d: Ready to bear off.\n",__LINE__);

      game_move move;

      move = get_fitting_bear_move(state, chosen_dice);
      if (!(move.point_from == 0 and move.roll == 0) and (!only_one_checker_left(state))) {
        // Apply move to state
        state->board[move.point_from] = state->player * ((state->player * state->board[move.point_from]) - 1);
        // Add chosen dice
        if (state->dice[0] == move.roll)
          chosen_dice.push_back(0);
        else if (state->dice[1] == move.roll)
          chosen_dice.push_back(1);
        else if (! (0 == move.roll)){
          printf("%d: Error in add dice: dice did not match roll.\n",__LINE__);
          abort();
        }
          // Add move to output
          output.push_back(move);
        }
        else if ((int)chosen_dice.size() < moves_left(state)){
          // Search for bear off move
          move = get_bear_move(state, chosen_dice);
          // printf("%d: Bear move is (%d,%d)\n",__LINE__,move.point_from,move.roll);
          // Returned move empty?
          if (!(move.point_from == 0 and move.roll == 0)) {
            // Add chosen dice
            if (state->dice[0] == move.roll)
              chosen_dice.push_back(0);
            else if (state->dice[1] == move.roll)
              chosen_dice.push_back(1);
            else if (! (0 == move.roll)){
              printf("%d: Error in add dice: dice did not match roll.\n",__LINE__);
              abort();
            }
            // Apply move to state
            state->board[move.point_from] = state->player * ((state->player * state->board[move.point_from]) - 1);
            // Add move to output
            output.push_back(move);
          }
          else {
            // Search for move but don't bear off
            move = get_normal_move(state, chosen_dice);
            // Add chosen dice
            if (state->dice[0] == move.roll)
              chosen_dice.push_back(0);
            else if (state->dice[1] == move.roll)
              chosen_dice.push_back(1);
            else if (! (0 == move.roll)) {
              printf("%d: Error in add dice: dice did not match roll. Revieved roll: %d\n",__LINE__,move.roll);
              abort();
            }
            // Returned move empty?
            if (!(move.point_from == 0 and move.roll == 0)) {
              // Apply move to state
              int target = modulo(move.point_from + (-1) * state->player * move.roll, 25);
              if (target <= 0) {
                printf("%d: Error in modulo. Target is %d.\n",__LINE__,target);
                abort();
              }
              if (enemies_blot(state, target))
                state->board[target] = state->player * ((state->player * state->board[target]) + 2);
              else
                state->board[target] = state->player * ((state->player * state->board[target]) + 1);
              state->board[move.point_from] = ((state->player * state->board[move.point_from]) - 1) * state->player;
              // Add move to output
              output.push_back(move);
            }
          }
        }
        // Decrement max moves
        max_moves = max_moves - 1;
      }
      else {

        // Search for move but don't bear off
        game_move move = get_normal_move(state, chosen_dice);

        // Add chosen dice
        if (state->dice[0] == move.roll)
          chosen_dice.push_back(0);
        else if (state->dice[1] == move.roll)
          chosen_dice.push_back(1);
        else if (! (0 == move.roll)) {
          printf("%d: Error in add dice: dice did not match roll. Revieved roll: %d\n",__LINE__,move.roll);
          abort();
        }

        // Returned move empty?
        if (!(move.point_from == 0 and move.roll == 0)) {

          // Apply move to state
          int target = modulo(move.point_from + (-1) * state->player * move.roll, 25);
          if (target <= 0) {
            printf("%d: Error in modulo. Target is %d.\n",__LINE__,target);
            abort();
          }
          if (enemies_blot(state, target))
            state->board[target] = state->player * ((state->player * state->board[target]) + 2);
          else
            state->board[target] = state->player * ((state->player * state->board[target]) + 1);

          state->board[move.point_from] = ((state->player * state->board[move.point_from]) - 1) * state->player;

          // Add move to output
          output.push_back(move);

        }
        // Decrement max moves
        max_moves = max_moves - 1;
      }
    }
  }
  return output;
}



/* EOF */
