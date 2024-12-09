<?php
$dir = __DIR__ . '/data';
$names = [];

if (is_dir($dir)) {
	$files = scandir($dir);
	foreach ($files as $file) {
		if (substr($file, -5) === '.name') {
			$name = substr($file, 0, -5);
			$age = trim(file_get_contents($dir . '/' . $file));
			$names[] = ['name' => $name, 'age' => $age];
		}
	}
}
?>
<!DOCTYPE html>
<html lang="en">

<head>
	<meta charset="UTF-8">
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<title>Welcome to My PHP Page</title>
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
		form {
			margin: 20px 0;
		}
		input[type="text"],
		input[type="number"] {
			padding: 10px;
			margin: 5px;
		}
		input[type="submit"] {
			background-color: #4CAF50;
			color: #fff;
			border: none;
			padding: 10px;
			cursor: pointer;
		}
		input[type="submit"]:hover {
			background-color: #45a049;
		}
		ul {
			padding: 0;
			list-style-type: none;
		}
		ul li{
			width: 50%;
			display: flex;
			gap: 20px;
			box-sizing: border-box;
			padding:10px;
			margin: auto;
			justify-content: space-between;
		}
		ul li:nth-of-type(1n){
			background-color: #ddd;
		}
		ul li:nth-of-type(2n){
			background-color: white;
		}
	</style>
</head>

<body>
	<header>
		Welcome to My PHP Page
	</header>
	<main>
		<h1>Hello, World!</h1>
		<p>Dies ist die Startseite des Webservers.</p>
		<p>Fühle dich frei, diese Seite zu erkunden und zu bearbeiten.</p>
		<h2>Namen und Alter:</h2>
		<form action="createName.php" method="post">
			<input type="text" name="name" placeholder="Name" required>
			<input type="number" name="age" placeholder="Alter" required>
			<input type="submit" value="Neuen Namen hinzufügen">
		</form>
		<ul>
			<?php foreach ($names as $entry): ?>
				<li><?php echo htmlspecialchars($entry['name']) . " (Alter: " . htmlspecialchars($entry['age']) . ")"; ?><button onclick="deleteName('<?php echo htmlspecialchars($entry['name']); ?>')" style="display: inline;">X</button></li>
			<?php endforeach; ?>
		</ul>
		<a href="/about.php">Mehr über uns</a>
		<script>
			function deleteName(name) {
				fetch("./data/" + name + ".name", {
					method: 'DELETE',
					headers: {
						'Content-Type': 'application/json',
					},
					body: JSON.stringify({ nameToDelete: name })
				})
				.then(response => {
					if (response.ok) {
						window.location.reload();
					}
				})
				.catch(error => console.error('Error:', error));
			}
		</script>
	</main>
</body>
</html>
