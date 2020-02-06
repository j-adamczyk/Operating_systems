#!/bin/bash

mkdir test_directories

for i in {1..5};
do
  echo $i > "test_directories/file$i.txt"
  cd test_directories
  ln -s "file$i.txt" sym_link_$i
  cd ..
done

for i in {1..5};
do
  mkdir "test_directories/directory$i"
  echo $i > "test_directories/directory$i/file$i.txt"
done
