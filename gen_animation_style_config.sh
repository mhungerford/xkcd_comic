#!/bin/bash
IFS=$'\n'
echo -n "" > resources/animation_style_config.txt
FIRST_FRAME=true
LASTFILE="junk_text"
IMAGES=( $(grep 'file' appinfo.json) )
for IMAGE in ${IMAGES[@]}; do
  if [[ $IMAGE =~ [0-9]-.*\.png ]]; then
    if ($FIRST_FRAME); then
      echo -n "0" >> resources/animation_style_config.txt
      FIRST_FRAME=false
    else
      if [[ $IMAGE =~ ${LASTFILE#*-} ]]; then
        FIRST_FRAME=false
        echo -n "1" >> resources/animation_style_config.txt
      else
        FIRST_FRAME=false
        echo -n "0" >> resources/animation_style_config.txt
      fi
    fi
  elif [[ $IMAGE =~ \.png ]]; then
    FIRST_FRAME=true
    echo -n "0" >> resources/animation_style_config.txt
  fi
  LASTFILE=$IMAGE
done

