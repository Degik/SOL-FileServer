#ifndef SOL_GENNAIO_API_H
#define SOL_GENNAIO_API_H

int openConnection(const char* sockname, int msec, const struct timespec abstime);
/*
 * Viene aperta una richiesta di connessione al server che se dovesse fallire
 * ripeterebbe dopo tot msec (millisecondi) fino allo scadere del tempo abstime
 * ritorna 0 in caso di successo, in caso di errore -1 (Viene settato errno)
 */

int closeConnection(const char* sockname);
/*
 * Chiude la connessione associata al sockname
 * ritorna 0 in caso di successo, in caso di errore -1 (Viene settato errno)
 */

int openFile(const char* pathname, int flags);
/*
 * Viene aperto o creato un file pathname, tutto questo in baso al valore del flag (O_CREATE or O_LOCK)
 * Se il valore del flag è O_CREATE, ma il file esiste già nel server viene ritornato un valore errno (errore)
 * Se il valore del flag O_CREATE non viene specificato, ma il file non esiste sul server ritorna un valore errno (errore)
 * In caso di successo il file viene sempre aperto in lettura e scrittura, sempre e solo in append
 * Se il file dovesse essere creato con il flag O_LOCK o in (or O_CREATE) allora sarebbe leggerlo e scriverlo solo
 * per il processo che l'ha aperto
 * ritorna 0 in caso di successo, in caso di errore -1 (Viene settato errno)
 */

int readFile(const char* pathname, void** buff, size_t* size);
/*
 * Legge il contenuto del file dal sever, se presente
 * Questo torna il puntatore ad un'areao allocato sullo heap (void** buff)
 * Torna le dimensioni in byte del file letto (size_t* size)
 * ritorna 0 in caso di successo, in caso di errore -1 (Viene settato errno)
 */

int readNFiles(int N, const char* dirname);
/*
 * Legge N file presenti all'interno del server
 * Se il server ha meno di N li manda tutti
 * Se N <= 0 il server legge tutto
 * ritorna 0 o il numero dei file letti in caso di successo, in caso di errore -1 (errno)
 */

int writeFile(const char* pathname);
/*
 * Scrive tutto il file pathname nel server
 * Se dirname != NULL il file spedito dal server per liberare la cache dovrà essere scritto in dirname
 * ritorna 0 in caso di successo, in caso di errore (Viene settato errno)
 */

int appendToFile(const char* pathname, void* buff, size_t size);
/*
 * Richiesta di scrivere in append nel file pathname(const char* pathname) i byte(size_t size) contenuti in buff(void* buff)
 * Se dirname != NULL allora il file eventualmente spedito dal server per fare pulizia della cache dovrà essere scritto in dirname
 * ritorna 0 in caso di successo, in caso di errore -1 (Viene settato errno)
 */

int lockFile(const char* pathname);
/*
 * In caso di succcesso setta il flag O_LOCK sul file
 * Se il file viene creato o aperto con O_LOCK allora la richiesta proviene dallo stesso processo e termina con successo
 * Altrimenti non viene completata fino a quando O_LOCK non viene resettato dal detentore della lock
 * ritorna 0 in caso di successo, in caso di errore -1 (Viene settato errno)
 */

int unlockFile(const char* pathname);
/*
 * Resetta il flag O_LOCK sul file pathname
 * L'operazione termina con successo se è il proprietario della lock a richiedere l'operazione altrimenti errore
 * ritorna 0 in caso di successo, in caso di errore(errno)
 */

int closeFile(const char* pathname);
/*
 * Viene chiuso il file
 * ritorna 0 in caso di successo, in caso di errore -1 (Viene settato errno)
 */

int removeFile(const char* pathname);
/*
 * Cancella il file dal server
 * L'operzione fallisce se il file non è in stato di lock
 * L'operzione fallisce se il file è in stato di lock, ma con proprietario diverso da quello che ha richeisto la remove
 */

#endif //SOL_GENNAIO_API_H
