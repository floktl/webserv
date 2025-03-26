<?php
session_start();
if (isset($_COOKIE["test_cookie"])) {
	$cookieval = intval($_COOKIE["test_cookie"]);
	$cookieval++;
} else {
	$cookieval = 0;
}

header('Cache-Control: no-cache');
setcookie('test_cookie', $cookieval, [
	'expires' => time() + 3600,
	'path' => '/',
	'httponly' => true
]);
header('Location: /');
exit();
?>
