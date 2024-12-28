#include <stdlib.h>
#include "chatmessagemodel.h"

ChatMessageModel::ChatMessageModel(QObject *parent)
    : QAbstractListModel(parent) {
}

ChatMessageModel::~ChatMessageModel()
{
    // すべての m_messages[i].role, .content を free
    for (auto & msg : m_messages) {
        free((void*)msg.role);    //  const_cast<void*>(...) とするか、C的には (char *)msg.role など
        free((void*)msg.content);
    }
    m_messages.clear();
}


QHash<int, QByteArray> ChatMessageModel::roleNames() const {
    static QHash<int, QByteArray> s_roleNames;
    if (s_roleNames.isEmpty()) {
        s_roleNames.insert(Sender, "sender");
        s_roleNames.insert(MessageContent, "messageContent");
    }
    return s_roleNames;
}

QVariant ChatMessageModel::data(const QModelIndex &index, int role) const {
    const int row = index.row();

    if (row < 0 || row >= m_messages.size()) {
        return QVariant();
    }

    const auto &data = m_messages.at(row);
    switch (role) {
    case Sender:
        return QVariant::fromValue(QString(data.role));
    case MessageContent:
        return QVariant::fromValue(QString(data.content));
    }

    return QVariant();
}

int ChatMessageModel::rowCount(const QModelIndex &) const {
    return m_messages.size();
}

void ChatMessageModel::append(const std::vector<llama_chat_message> &messages) {
    if (messages.size() > 0) {
        beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size() + messages.size() - 1);
        m_messages.insert(m_messages.end(), messages.begin(), messages.end());
        endInsertRows();
    }
}

int ChatMessageModel::appendSingle(const QString &sender, const QString &content)
{
    // 追加するメッセージを1件作る
    llama_chat_message msg;
    // 文字列の管理方法は、既存のappend()の中で strdup しているので合わせる
    // 例: msg.role = strdup(sender.toStdString().c_str());
    //     msg.content = strdup(content.toStdString().c_str());
    msg.role    = strdup(sender.toUtf8().constData());
    msg.content = strdup(content.toUtf8().constData());

    // 新規行を1つだけ追加するので、startIndex = endIndex = m_messages.size()
    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.push_back(msg);
    endInsertRows();

    // 追加された行のindexを返す
    return static_cast<int>(m_messages.size()) - 1;
}


void ChatMessageModel::updateMessageContent(int row, const QString &newContent)
{
    if (row < 0 || row >= static_cast<int>(m_messages.size())) {
        return; // out of range
    }

    // 古い content を開放して新しい文字列に差し替え（strdup管理の場合）
    free((void*)m_messages[row].content);
    m_messages[row].content = strdup(newContent.toUtf8().constData());

    // dataChanged で QML の再描画をトリガ
    QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {MessageContent});
}

