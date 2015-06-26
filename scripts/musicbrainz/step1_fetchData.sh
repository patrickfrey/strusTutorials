#!/bin/sh

echo "Prepare data directory ..."
mkdir data
cd data
echo "Download and uppack the data dump ..."
wget http://ftp.musicbrainz.org/pub/musicbrainz/data/fullexport/20150624-002847/mbdump-cdstubs.tar.bz2
bzip2 -d mbdump-cdstubs.tar.bz2
tar -xvf mbdump-cdstubs.tar
ndocs=`cat mbdump/release_raw | awk -F"\t" '{if(min=="")min=max=$1; if($1>max) {max=$1}; if($1< min) {min=$1}; } END {print int(max/100)}'`
echo "Create the documents ..."
mkdir doc
idoc=0
while [ $idoc -lt $ndocs ]
do
    echo "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?>" > doc/$idoc.xml
    idoc=`expr $idoc + 1`
done
echo "Fill the documents with the content of the dump ..."
cat mbdump/release_raw\
  | awk -F"\t" '{FNAME=int($1/100); print "<item><id>" $1 "</id><title>" $2 "</title><artist>" $3 "</artist><date>" $4 "</date><upc>" $9 "</upc><note>" $10 "</note></item>" >> "doc/"FNAME".xml" }'

cd ..
echo "Leave the data directory and inspect the result ..."
echo "Nof Documents: `ls -l data/doc/*.xml | wc -l`"
echo "Nof entries (example document) `cat data/doc/999.xml | grep item | wc -l`"





