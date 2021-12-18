let total = 0;

for t in ./tests/*.o; do
    echo "Running test : $t"
    ./t;
    let total++;
done

echo "Total tests = $total"
echo "Done."