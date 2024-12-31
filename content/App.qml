// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 6.2
import QllamaTalk

Window {
    width: Screen.width
    height: Screen.height

    visible: true
    title: "QllamaTalk"

    ChatView {
        id: mainScreen
    }
}

