set -e

name=${1?Product name wasn\'t specified}

awk '
    /Returning pathname .*\/'$name'/ {print}
    /Multicasted product .*name="'$name'"/ {
        if (match($0, /prodId=/)) {
            prodId=substr($0, RSTART+7, 16);
            print "prodId=\"" prodId "\"";
            haveProdId=1;
        }
    }
    {
        if (haveProdId && index($0, prodId)) print;
    }
'
