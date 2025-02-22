import os
import re
from deep_translator import GoogleTranslator

def translate_comment(comment):
    """Translate a German comment to English using Google Translator."""
    try:
        return GoogleTranslator(source='de', target='en').translate(comment)
    except Exception as e:
        print(f"Translation error: {e}")
        return comment  # Return the original if translation fails

def process_file(file_path):
    """Read a file, translate German comments, and rewrite the file."""
    with open(file_path, 'r', encoding='utf-8') as file:
        lines = file.readlines()

    updated_lines = []
    comment_pattern = re.compile(r'^\s*//(.*)')  # Match lines that start with //

    for line in lines:
        match = comment_pattern.match(line)
        if match:
            german_comment = match.group(1).strip()
            translated_comment = translate_comment(german_comment)
            new_line = f"// {translated_comment}\n"
            print(f"Translating: {german_comment} -> {translated_comment}")
        else:
            new_line = line

        updated_lines.append(new_line)

    with open(file_path, 'w', encoding='utf-8') as file:
        file.writelines(updated_lines)

def search_and_translate(directory):
    """Find all .cpp and .hpp files in a directory and translate their comments."""
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith(('.cpp', '.hpp')):
                file_path = os.path.join(root, file)
                print(f"Processing: {file_path}")
                process_file(file_path)

if __name__ == "__main__":
    project_directory = os.getcwd()  # Change this if needed
    search_and_translate(project_directory)
