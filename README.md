# Messages
Academic project on C (for Linux).

Client Server architecture to send and receive messages, organized by topics. 

A user can send messages to a given topic and subscribe to one or more topics. Whenever a message is sent to
a given topic, users who subscribed to it receive that message. There will be a “manager” program that
manages the reception and distribution of messages, and there will be another “feed” program (sends and receives) that is used by
users.
