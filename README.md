# Εργασία 3 – Σύστημα κράτησης εισιτηρίων

##### Εαρινό εξάμηνο 2017-2018

Υλοποιήστε ένα σύστημα κράτησης αεροπορικών εισιτηρίων το οποίο εκτελείται τοπικά σε έναν υπολογιστή. Ένας
server (εξυπηρετητής) δέχεται αιτήματα από πράκτορες για πρόσβαση σε κοινόχρηστη μνήμη που περιλαμβάνει
πληροφορίες πτήσεων και διαθέσιμων θέσεων. Επίσης δέχεται ενημερώσεις για τις κρατήσεις που κάνουν οι
διάφοροι πράκτορες. Κάθε πράκτορας, εφόσον το αίτημά του γίνει δεκτό, αποκτά πρόσβαση στην κοινόχρηστη
μνήμη την οποία μπορεί να διαβάσει για να ενημερωθεί για διαθέσιμες πτήσεις ή να γράψει σε περίπτωση
κράτησης εισιτηρίων. Μπορεί να υπάρχουν ταυτόχρονα πολλοί ενεργοί πράκτορες αλλά η ανάγνωση από / εγγραφή
στην κοινόχρηστη μνήμη πρέπει να γίνεται υπό αμοιβαίο αποκλεισμό με την χρήση σηματοφόρων.

### Server

Το πρόγραμμα παίρνει ως ορίσματα το μέγιστο αριθμό πρακτόρων που μπορεί να εξυπηρετήσει, και το όνομα ενός
αρχείου που περιέχει πληροφορίες πτήσεων. Δημιουργεί την κοινόχρηστη μνήμη και αποθηκεύει σε αυτή (σε
δυαδική μορφή) τις πληροφορίες που διαβάζει από το αρχείο. Δημιουργεί επίσης έναν ή περισσότερους
σηματοφόρους για τον συγχρονισμό ανάγνωσης/εγγραφής στην κοινόχρηστη μνήμη.

Ο server διατηρεί ένα file descriptor από τον οποίο διαβάζει τα αιτήματα των πρακτόρων (κάθε αίτημα περιέχει το
process id του πράκτορα). Εφόσον δεν είναι συνδεδεμένος ο μέγιστος αριθμός πρακτόρων, το αίτημα γίνεται δεκτό,
ο server στέλνει στον πράκτορα ότι πληροφορία χρειάζεται ώστε να μπορεί να προσπελάσει την κοινόχρηστη
μνήμη, και εκτυπώνει μήνυμα με το process id του πράκτορα. Επιπλέον, για κάθε πράκτορα που γίνεται δεκτός, ο
server δημιουργεί/διατηρεί ξεχωριστό file descriptor από τον οποίο διαβάζει πληροφορία για κάθε κράτηση που
πραγματοποιείται, και εκτυπώνει μήνυμα με το process id του πράκτορα και τον αριθμό θέσεων της κράτησης.

Παράλληλα, ο server παρακολουθεί τη συμβατική του είσοδο. Αν διαβάσει το μήνυμα “Q” ή ανιχνεύσει
end-of-input, εκτυπώνει στην οθόνη το συνολικό αριθμό θέσεων που κρατήθηκαν από κάθε πράκτορα που είναι
ακόμη ενεργός, ανανεώνει τα περιεχόμενα του αρχείου (ανάλογα με τις κρατήσεις που έχουν πραγματοποιηθεί),
κλείνει όλους τους file descriptors και μόνιμες οντότητες IPC που δημιούργησε, και τερματίζει.

Το πρόγραμμα πρέπει να παρακολουθεί όλους τους file descriptors χωρίς ενεργή αναμονή (μέσω select ή poll).

### Πράκτορας

Κάθε διεργασία-πράκτορας εκτελεί το ίδιο πρόγραμμα το οποίο δέχεται ως όρισμα τα στοιχεία επικοινωνίας του
server. Κατά την εκκίνηση στέλνει ένα αίτημα στο server με το process id του. Εάν τον αίτημα δε γίνει δεκτό,
τερματίζει. Διαφορετικά, δέχεται επαναληπτικά από το πληκτρολόγιο αιτήσεις από το χρήστη, οι οποίες μπορεί να
είναι για εύρεση διαθέσιμων πτήσεων (FIND), για κράτηση εισιτηρίων (RESERVE) ή για τερματισμό (EXIT).

Μια αίτηση για εύρεση πτήσεων είναι της μορφής FIND SRC DEST NUM όπου SRC και DEST τα αεροδρόμια
αναχώρησης και άφιξης αντίστοιχα και NUM το πλήθος εισιτηρίων που ζητούνται. Το πρόγραμμα αναζητά στην
κοινόχρηστη μνήμη πτήσεις που ικανοποιούν τις απαιτήσεις και τις εμφανίζει στην οθόνη.

Μια αίτηση για κράτηση είναι της μορφής RESERVE SRC DEST AIRLINE NUM όπου AIRLINE είναι η
επιθυμητή αεροπορική εταιρία και τα υπόλοιπα όπως περιγράφεται παραπάνω. Το πρόγραμμα αναζητά τη
συγκεκριμένη πτήση, κι εφόσον υπάρχουν ακόμη αρκετές θέσεις, αφαιρεί το NUM από το πλήθος διαθέσιμων
θέσεων, στέλνει το NUΜ στον server, και εκτυπώνει μήνυμα επιτυχίας στην οθόνη. Αν δε βρεθεί η ζητούμενη
πτήση ή βρεθεί αλλά δεν υπάρχουν αρκετές θέσεις, εκτυπώνει μήνυμα αποτυχίας.

###### Μορφή αρχείου με πληροφορίες πτήσεων

Το αρχείο δίνεται ως κείμενο ASCII όπου κάθε γραμμή τα στοιχεία μιας πτήσης, και για την ακρίβεια τις παρακάτω
πληροφορίες χωρισμένες με ένα κενό: κωδικός αεροπορικής εταιρίας (μέχρι 3 χαρακτήρες), κωδικός αεροδρομίου
αναχώρησης (μέχρι 4 χαρακτήρες), κωδικός αεροδρομίου άφιξης (μέχρι 4 χαρακτήρες), πλήθος ενδιάμεσων
στάσεων (ακέραιος), πλήθος θέσεων στο αεροσκάφος (ακέραιος).
