# SPDX-License-Identifier: MIT
# M11d — 分阶段独立提交脚本（Bash 恢复后由 Agent 逐段执行；也可手动运行）
# 用法: 在 D:\projects\spike-elite 下运行  powershell -ExecutionPolicy Bypass -File tools\m11d_git_commits.ps1
# 注意: 每阶段应先在 Agent 中完成对应编译验证后再提交本阶段文件。

$ErrorActionPreference = "Stop"
Set-Location "D:\projects\spike-elite"

function CommitStage([string]$Msg, [string[]]$Paths) {
    git add -- $Paths
    $staged = git diff --cached --name-only
    if (-not $staged) { Write-Host "SKIP (no changes): $Msg"; return }
    git commit -m $Msg
    Write-Host "COMMIT OK: $Msg"
}

# M11d-1: 统一设计令牌
CommitStage "M11d-1: unified UI design tokens (SEUiStyle palette/type/spacing/anim, focus rim, reduced-motion flag)" @(
    "Source/SpikeElite/Public/UI/SEUiStyle.h"
)

# M11d-2: 菜单 / 设置 / 暂停 / 确认 / 比赛结束 重构
CommitStage "M11d-2: navy broadcast main menu, sectioned settings with UI scale + reduced motion, tokenized pause/confirm, match-end hierarchy" @(
    "Source/SpikeElite/Public/UI/MainMenuWidget.h",
    "Source/SpikeElite/Private/UI/MainMenuWidget.cpp",
    "Source/SpikeElite/Private/UI/PauseMenuWidget.cpp",
    "Source/SpikeElite/Private/UI/ConfirmWidget.cpp",
    "Source/SpikeElite/Public/UI/MatchEndWidget.h",
    "Source/SpikeElite/Private/UI/MatchEndWidget.cpp",
    "Source/SpikeElite/Public/UI/SettingsWidget.h",
    "Source/SpikeElite/Private/UI/SettingsWidget.cpp"
)

# M11d-3: 比赛 HUD（镜像比分板 + 小球场轮转）
CommitStage "M11d-3: broadcast mirrored scoreboard with stage badge/touch dots/banner, mini-court rotation HUD, bJustRotated from GameMode" @(
    "Source/SpikeElite/Public/UI/ScoreboardWidget.h",
    "Source/SpikeElite/Private/UI/ScoreboardWidget.cpp",
    "Source/SpikeElite/Public/UI/RotationWidget.h",
    "Source/SpikeElite/Private/UI/RotationWidget.cpp",
    "Source/SpikeElite/Public/Volleyball/VolleyballRules.h",
    "Source/SpikeElite/Public/SpikeEliteGameMode.h",
    "Source/SpikeElite/SpikeEliteGameMode.cpp"
)

# M11d-4: 战术 UI 重排
CommitStage "M11d-4: tactical HUD re-layout — attack card (target zone/power bar/arc tier), set list in ScrollBox with category, defense two-column" @(
    "Source/SpikeElite/Public/UI/TacticalHUDWidget.h",
    "Source/SpikeElite/Private/UI/TacticalHUDWidget.cpp"
)

# M11d-5: 场馆美术
CommitStage "M11d-5: arena polish — two-tone acoustic panels, un-branded SPIKE ELITE/PLAY FAIR signage" @(
    "Source/SpikeElite/Private/Volleyball/VolleyballArena.cpp"
)

# M11d-6: 角色与球
CommitStage "M11d-6: stylized athlete (shoulder pads/hip block, jersey numbers) and un-branded yellow/blue/white multi-panel ball" @(
    "Source/SpikeElite/Public/SpikeEliteCharacter.h",
    "Source/SpikeElite/SpikeEliteCharacter.cpp",
    "Source/SpikeElite/Public/Volleyball/VolleyballBall.h",
    "Source/SpikeElite/Private/Volleyball/VolleyballBall.cpp"
)

# M11d-7: 文档
CommitStage "M11d-7: docs — changelog/readme/roadmap/art-direction/ball license updated for M11d" @(
    "CHANGELOG.md",
    "README.md",
    "docs/05-roadmap.md",
    "docs/06-art-direction.md",
    "Content/Balls/ASSET_LICENSE.md"
)

Write-Host "`n=== git status ==="
git status --short --branch
