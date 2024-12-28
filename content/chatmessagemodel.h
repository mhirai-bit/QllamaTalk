#ifndef CHATMESSAGEMODEL_H
#define CHATMESSAGEMODEL_H

#include <QObject>
#include <QAbstractListModel>
#include "llama.h"

class ChatMessageModel: public QAbstractListModel {
    Q_OBJECT
public:
    explicit ChatMessageModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role) const override;

    void append(const std::vector<llama_chat_message> &messages);

private:
    std::vector<llama_chat_message> m_messages;
    enum Role {
        Sender = Qt::UserRole + 1,
        MessageContent,
    };
};
#endif // CHATMESSAGEMODEL_H
