#!/bin/bash

mkdir test_directories
cd test_directories
mkdir small_directories
mkdir medium_directories
mkdir large_directories

for i in {1..10};
do
  echo $i > "small_directories/file$i.txt"
done

for i in {1..10};
do
  mkdir "small_directories/directory$i"
  echo $i > "small_directories/directory$i/file$i.txt"
done

for i in {1..10};
do
  mkdir "medium_directories/directory$i"
  echo $i > "medium_directories/directory$i/file$i.txt"

  for k in {1..10};
  do
    mkdir "medium_directories/directory$i/dir$k"
    echo $i > "medium_directories/directory$i/dir$k/file$k.txt"
  done
done

for i in {1..300};
do
  mkdir "large_directories/directory$i"
  echo $i > "large_directories/directory$i/file$i.txt"

  for k in {1..10};
  do
    mkdir "large_directories/directory$i/directory$k"
    echo $i > "large_directories/directory$i/directory$k/file$k.txt"
  done
done

cd ..

