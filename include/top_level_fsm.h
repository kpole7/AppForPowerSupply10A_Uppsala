/// @file top_level_fsm.h
/// @brief Declaration of the top-level finite state machine (FSM) for the application.
/// @details This file contains the interface for managing the sequencies of commands that control
/// the external device.

#ifndef TOP_LEVEL_FSM_H
#define TOP_LEVEL_FSM_H

void initializeTopLevelFsm();

void topLevelFsmTick(bool TerminationSignal);

#endif // TOP_LEVEL_FSM_H