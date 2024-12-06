package main

import (
	"fmt"
)



func main() {
	fmt.Print(`
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Simple HTML Page</title>
    </head>
    <body>
        <h1>Welcome to a simple Web Page!</h1>
        <p>This is rendered by go!</p>
    </body>
    </html>
`);
}
