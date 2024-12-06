<?php
$names = ["Charlie", "Barny", "Fred"];
?>

<!DOCTYPE html>
<html lang="en">

<head>
	<meta charset="UTF-8">
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<title>Welcome to My Web Server</title>
	<style>
		body {
			font-family: Arial, sans-serif;
			text-align: center;
			background-color: #f4f4f9;
			margin: 0;
			padding: 0;
		}

		header {
			background-color: #4CAF50;
			color: white;
			padding: 20px 0;
			font-size: 2em;
		}

		main {
			padding: 20px;
		}

		a {
			color: #4CAF50;
			text-decoration: none;
			font-weight: bold;
		}

		a:hover {
			text-decoration: underline;
		}
	</style>
</head>

<body>
	<header>
		Welcome to My Web Server
	</header>
	<main>
		<h1>Hello, World!</h1>
		<p>This is the home page of your web server.</p>
		<p>Feel free to explore and modify this page.</p>
		<h2>Some names rendered by PHP:</h2>
		<ul>
			<?php for ($i = 0 ; $i < sizeof($names) ; $i++): ?>
				<li><?php echo $names[$i]; ?></li>
			<?php endfor; ?>
		</ul>
		<a href="/about.html">Learn more about us</a>
	</main>
</body>

</html>