#pragma once

// Milestone 10: minimal crash logging.
//
// RNF-009 is already covered for failures the code can see - a lost device, a capture that
// will not start, a corrupt settings file all get caught, logged and recovered from. What
// none of that catches is a structural failure: an access violation walks straight past
// every catch(...) in the program and the process disappears with nothing written down.
//
// This installs a last-resort handler so that case leaves a record behind.

#include <filesystem>

namespace overlaydesk {

// Writes crash records and minidumps into `crashDirectory`. Safe to call once at startup,
// before anything else can fault.
void InstallCrashHandler(const std::filesystem::path& crashDirectory);

}  // namespace overlaydesk
