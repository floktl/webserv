<?php
error_reporting(E_ALL);
ini_set('display_errors', 1);

ob_start();

if ($_SERVER['REQUEST_METHOD'] === 'DELETE') {
	$data = json_decode(file_get_contents('php://input'), true);
	$nameToDelete = isset($data['nameToDelete']) ? $data['nameToDelete'] : '';

	$nameToDelete = preg_replace('/[^a-zA-Z0-9_-]/', '', $nameToDelete);

	if (!empty($nameToDelete)) {
		$dir = __DIR__ . '/data';
		$filePath = $dir . '/' . $nameToDelete . '.name';

		if (file_exists($filePath)) {
			if (unlink($filePath)) {
				http_response_code(200);
				echo json_encode(['status' => 'success']);
			} else {
				http_response_code(500);
				echo json_encode(['error' => 'Failed to delete file']);
			}
		} else {
			http_response_code(404);
			echo json_encode(['error' => 'File not found']);
		}
	} else {
		http_response_code(400);
		echo json_encode(['error' => 'Invalid name']);
	}
} else {
	http_response_code(405);
	echo json_encode(['error' => 'Method not allowed']);
}

ob_end_flush();