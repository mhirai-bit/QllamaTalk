#ifndef CHATMESSAGEMODEL_H
#define CHATMESSAGEMODEL_H

#include <QObject>
#include <QAbstractListModel>
#include "llama.h"

class ChatMessageModel: public QAbstractListModel {
    Q_OBJECT
public:
    explicit ChatMessageModel(QObject *parent = nullptr);
    ~ChatMessageModel() override;
    int rowCount(const QModelIndex &parent) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role) const override;

    void append(const std::vector<llama_chat_message> &messages);
    int appendSingle(const QString &sender, const QString &content);
    void updateMessageContent(int row, const QString &newContent);

private:
    std::vector<llama_chat_message> m_messages;
    enum Role {
        Sender = Qt::UserRole + 1,
        MessageContent,
    };
};
#endif // CHATMESSAGEMODEL_H
