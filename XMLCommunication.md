# Introduction #


The communication between the server and client is done through a socket-connection sending null-terminated xml-messages.
A typical connection may look like this:
Server:
> `<authorization>432302543GAGHTEAHAFWABRSE4535gFREWAF</authorization> `
Client:
> `<authorization username="doep">GRGRAEHTEAG4334575FDSAG</authorization> `
Client requesting all genres and artists:
> ` <query type="ListSelection" id="3" uid="2"><listeners>genre,artist</listeners></query> `
Server sending response to query:
> ` <queryresults id="3" uid="2"><metadata key="genre"><md id="3">Pop</md><md id="4">Rock</md><md id="5">Vocal</md></metadata><metadata key="artist"><md id="3">Bob</md><md id="4">Beatles</md><md id="5">Depeche</md></metadata></queryresults> `