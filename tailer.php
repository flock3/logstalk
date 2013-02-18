<?php

require 'src/pheanstalk/pheanstalk_init.php';

$file = 'file.txt';

$previousSize = filesize($file);

$iteration = 0;
$alerts = array(); // Found alerts
$lastDumpTime = time();

$pheanstalk = new Pheanstalk_Pheanstalk('127.0.0.1');
$pheanstalk->useTube('raw-alerts');
while(true)
{
    clearstatcache();
    ++$iteration;

    $currentSize = filesize($file);

    echo "Starting iteration " . $iteration . ' with a filesize of: ' . $currentSize . PHP_EOL;

    if($currentSize == $previousSize)
    {
        sleep(1);
        continue;
    }

    if($currentSize < $previousSize)
    {
        $previousSize = 0;
        continue;
    }

    echo 'Fopening the file for reading.. ';

    $contents = fopen($file, 'r');

    echo 'Seeking to position.. ';
    fseek($contents, $previousSize);

    echo 'Reading data into buffer.. ';

    $data = fread($contents, ($currentSize - $previousSize));

    fclose($contents);

    echo 'Closing the file.. ' . PHP_EOL;

    $previousSize = $currentSize;

    $matchingAlertHandle = '** Alert';

    $foundPositions = array();
    $offset = 0;
    while(true)
    {
        $position = stripos($data, $matchingAlertHandle, $offset);

        if(false === $position)
            break;

        $offset = $position + strlen($matchingAlertHandle);
        $foundPositions[] = $position;
    }

    $previousPosition = -1;

    foreach($foundPositions as $position)
    {
        if(-1 == $previousPosition)
        {
            $previousPosition = $position;
            continue;
        }

        $alert = substr($data, $previousPosition, ($position - $previousPosition));

        $alerts[] = $alert;
    }

    $alerts[] = substr($data, $position);

    if(count($alerts) > 20 || (time() - $lastDumpTime) > 20)
    {
        echo "\tPutting the current " . count($alerts) . ' into beanstalk.' . PHP_EOL;
        $alertChunks = array_chunk($alerts, 10);

        foreach($alertChunks as $chunk)
            $pheanstalk->put(base64_encode(gzcompress(json_encode($chunk))));

        unset($alerts);
    }

    unset($foundPositions, $position, $chunk, $alertChunks, $alert, $offset, $matchingAlertHandle);

    sleep(1);

}
