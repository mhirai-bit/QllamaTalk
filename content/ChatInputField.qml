// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Alexey Edelev <semlanik@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtQuick.Controls

TextField {
    id: _inputField
    width: 200
    color: "#cecfd5"
    placeholderTextColor: "#9d9faa"
    font.pointSize: 14
    padding: 10

    // 入力中の仮名変換などでハイライト表示される色を明示的に指定する
    selectionColor: "#6C71C4"       // ハイライト部分の色 (例: 紫っぽい色)
    selectedTextColor: "#ffffff"    // ハイライト部分にある文字色 (例: 白)

    background: Rectangle {
        radius: 5
        border {
            width: 1
            color: _inputField.focus ? "#41cd52" : "#f3f3f4"
        }
        color: "#222840"
    }
}
