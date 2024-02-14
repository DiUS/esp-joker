#!/bin/bash
infile="$1"
arrname="$2"
outfile="$3"
echo "const char *${arrname}[] = {" > "${outfile}"
while read -r line
do
  line=$(echo "${line}" | sed -e 's/"/'\''/g' -e 's/<>/;;; /')
  echo "  \"${line}\"," >> "${outfile}"
done < "${infile}"
echo "};" >> "${outfile}"
