<?php
session_start();
header('Cache-Control: no-cache');
header('Set-Cookie: test_cookie=Take a cookie and shove it; Max-Age=3600; Path=/; HttpOnly', false);
header('Location: /');
exit();
?>