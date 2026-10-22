// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Wicked Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Wicked Launcher Contributors
 */

#pragma once

#include "minecraft/auth/MinecraftAccount.h"

class QWidget;

//! Creates a new offline account with custom UUID, skin, and cape, or edits an existing one. Returns nullptr if cancelled.
MinecraftAccountPtr createEnhancedOfflineAccount(QWidget* parent, MinecraftAccountPtr existingAccount = nullptr);
