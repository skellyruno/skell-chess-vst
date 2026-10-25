#pragma once
#include <JuceHeader.h>
namespace checkmate
{
inline const juce::StringArray archiveNames = {"The Immortal Game / 1851", "Game of the Century / 1956",
                                               "The Opera Game / 1858", "Deep Blue vs Kasparov / 1997",
                                               "Queen's Gambit / fictional"};
inline const juce::StringArray archiveMoves[] = {
    {"e4", "e5",    "f4",  "exf4", "Bc4",   "Qh4+", "Kf1",  "b5",   "Bxb5", "Nf6",  "Nf3", "Qh6",
     "d3", "Nh5",   "Nh4", "Qg5",  "Nf5",   "c6",   "g4",   "Nf6",  "Rg1",  "cxb5", "h4",  "Qg6",
     "h5", "Qg5",   "Qf3", "Ng8",  "Bxf4",  "Qf6",  "Nc3",  "Bc5",  "Nd5",  "Qxb2", "Bd6", "Bxg1",
     "e5", "Qxa1+", "Ke2", "Na6",  "Nxg7+", "Kd8",  "Qf6+", "Nxf6", "Be7#"},
    {"Nf3",  "Nf6",  "c4",   "g6",    "Nc3", "Bg7",  "d4",  "O-O",   "Bf4",  "d5",   "Qb3",  "dxc4",
     "Qxc4", "c6",   "e4",   "Nbd7",  "Rd1", "Nb6",  "Qc5", "Bg4",   "Bg5",  "Na4",  "Qa3",  "Nxc3",
     "bxc3", "Nxe4", "Bxe7", "Qb6",   "Bc4", "Nxc3", "Bc5", "Rfe8+", "Kf1",  "Be6",  "Bxb6", "Bxc4+",
     "Kg1",  "Ne2+", "Kf1",  "Nxd4+", "Kg1", "Ne2+", "Kf1", "Nc3+",  "Kg1",  "axb6", "Qb4",  "Ra4",
     "Qxb6", "Nxd1", "h3",   "Rxa2",  "Kh2", "Nxf2", "Re1", "Rxe1",  "Qd8+", "Bf8",  "Nxe1", "Bd5",
     "Nf3",  "Ne4",  "Qb8",  "b5",    "h4",  "h5",   "Ne5", "Kg7",   "Kg1",  "Bc5+", "Kf1",  "Ng3+",
     "Ke1",  "Bb4+", "Kd1",  "Bb3+",  "Kc1", "Ne2+", "Kb1", "Nc3+",  "Kc1",  "Rc2#"},
    {"e4",    "e5",  "Nf3",  "d6",   "d4",  "Bg4", "dxe5",  "Bxf3", "Qxf3", "dxe5",  "Bc4",
     "Nf6",   "Qb3", "Qe7",  "Nc3",  "c6",  "Bg5", "b5",    "Nxb5", "cxb5", "Bxb5+", "Nbd7",
     "O-O-O", "Rd8", "Rxd7", "Rxd7", "Rd1", "Qe6", "Bxd7+", "Nxd7", "Qb8+", "Nxb8",  "Rd8#"},
    {"e4",  "c6",   "d4",   "d5",   "Nc3",  "dxe4", "Nxe4", "Nd7",  "Ng5",  "Ngf6", "Bd3", "e6",  "N1f3",
     "h6",  "Nxe6", "Qe7",  "O-O",  "fxe6", "Bg6+", "Kd8",  "Bf4",  "b5",   "a4",   "Bb7", "Re1", "Nd5",
     "Bg3", "Kc8",  "axb5", "cxb5", "Qd3",  "Bc6",  "Bf5",  "exf5", "Rxe7", "Bxe7", "c4"},
    {"d4",   "d5",  "c4",   "dxc4", "e4",   "Nc6",  "Be3",  "Nf6",   "Nc3",  "e5",  "d5",  "Ne7",
     "Bxc4", "Ng6", "f3",   "Bd6",  "Qd2",  "Bd7",  "Nge2", "a6",    "O-O",  "b5",  "Bd3", "O-O",
     "a4",   "b4",  "Nd1",  "c6",   "dxc6", "Bxc6", "Nf2",  "Qe7",   "Rfc1", "Bd7", "Bc4", "Rfd8",
     "Qe1",  "Be6", "Bxe6", "Qxe6", "Nd3",  "Qb3",  "Qd1",  "Qxd1+", "Rxd1", "Bf8", "b3",  "Rac8"},
};
} // namespace checkmate
