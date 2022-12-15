# Operativsystem Lab 3


## Systemspecification
fs är en lista med storleken no_blocks, vilket är 2048 (disk.get_no_blocks())
listan är av typ unsigned int 16-bit, och pekar antingen på ett annat block, FREE eller EOF
alla block har storleken 4096 (disk.BLOCK_SIZE)

## Icke standard block
block 0 har vårt root system, som har flera direntries, en för varje folder.
ett direntry har storleken filename(56 bytes) + size (4 bytes) + first_blk (2 bytes) + type (1 byte) + access_rights (1 byte) = 64 bytes
vi får då plats med 4096/64 = 64 filer/directories per block.


block 1 är resarverat för fs, eftersom blocket har 4096 bytes, så har vi 4096/2048(no_blocks) = 2 som ger oss 2 bytes eller 16 bitar för varje block.
om vi har en 16-bit signed int kan via ha upp till 32768 block om vi vill. Vi behöver dock bara 2048 för detta filsystem.

## Filesystem förklaring
fs värden:<br/>
2-2047		: diskspace<br/>
0		: free<br/>
-1		: EOF<br/>
2048 - 32768	: Invalid block number<br/>
-32768 - (-2) 	: Invalid block number 

## Förklaring av direntry
type = 1 för directory (subdirectory) eller 0 för file.
alla directories har block_status = EOF / -1 i fs
en fil/subdirectory finns vid first_blk, en signed int som pekar på ett annat block.
det är där man läser directory eller första 4096 bytes av filen.
size är storleken för en fil, eller 4096(?) för ett directory

access rights är en 8 bitars int där 4-biten representerar read, 2-biten representerar write, 1-biten representerar execute.<br/>
000 = 0, inga rättigheter<br/>
001 = 1, exekvera<br/>
010 = 2, skriv<br/>
011 = 3, skriv, exekvera<br/>
100 = 4, läs<br/>
101 = 5, läs, exekvera<br/>
110 = 6, läs, skriv<br/>
111 = 7, alla rättigheter<br/>

För att få ut en viss accesright så kan man ta (access_right & (RIGHT))/RIGHT för att få 1 eller 0.
