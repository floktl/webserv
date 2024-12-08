if __name__ == "__main__":
    people = ["Alice", "Bob", "Charlie", "Diana"]

    print("""
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Simple HTML Page</title>
    </head>
    <body>
        <h1>Welcome to a simple Web Page!</h1>
        <p>This is rendered by python!</p>
        <ul>
    """)

    for person in people:
        print(f"            <li>{person}</li>")

    print("""
        </ul>
    </body>
    </html>
    """)
