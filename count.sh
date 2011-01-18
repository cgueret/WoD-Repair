zcat data/network/profiles-tfidf-clear.csv.gz | awk '{ print $2 }' | sed -e 's/^\([^ /]*\)\/.*/\1/' | sort | uniq | wc -l
