<?php
$dir = __DIR__ . '/data';
if (!is_dir($dir)) {
	mkdir($dir, 0777, true);
}

$name = preg_replace('/[^a-zA-Z0-9_-]/', '', $_POST['name']);
$age = (int)$_POST['age'];

// $i = 10;
// while($i > 0)
// {
// 	sleep(10);
// 	$i--;
// }

if (!empty($name) && $age > 0) {
	file_put_contents($dir . '/' . $name . '.name', $age);
}

header('Location: /');
exit();
?>