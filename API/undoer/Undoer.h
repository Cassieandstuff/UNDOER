#pragma once
/// @file
/// UNDOER — one undo core for every document type: a shared step stack (Stack) that a step can span documents of any format on,
/// each document's part in it (DocumentHistory), and the two ready-made ways to capture a change: per item (ItemHistory) and whole
/// document (SnapshotHistory). Plus a typed clipboard. Standard library only.

#include <undoer/Clipboard.h>
#include <undoer/DocumentHistory.h>
#include <undoer/ItemHistory.h>
#include <undoer/Ref.h>
#include <undoer/SnapshotHistory.h>
#include <undoer/Stack.h>
