# main.py

# To run this server, you need to install the libraries first:
# pip install Flask google-generativeai gunicorn

from flask import Flask, request, Response
import google.generativeai as genai
import os
import logging

# --- Setup ---
# It's highly recommended to set your API key as an environment variable
# for security reasons, rather than writing it directly in the code.
# On your server (like Render), you will set this key in the environment variables section.
try:
    GEMINI_API_KEY = os.environ.get("GEMINI_API_KEY")
    if not GEMINI_API_KEY:
        # This will be logged on the server if the key is missing.
        logging.warning("GEMINI_API_KEY environment variable not set.")
    genai.configure(api_key=GEMINI_API_KEY)
except Exception as e:
    logging.error(f"Error configuring Gemini: {e}")
    GEMINI_API_KEY = None # Ensure it's None if setup fails

# Configure logging to see messages on the server
logging.basicConfig(level=logging.INFO)

# Initialize the Flask application
app = Flask(__name__)

# --- Gemini Model Configuration ---
# We use a dictionary to manage multiple conversations, one for each caller.
# This way, the bot can remember the context for each user separately.
conversations = {}

def get_chat_session(session_id):
    """
    Retrieves or creates a chat session for a given session_id (caller_id).
    """
    if session_id not in conversations:
        logging.info(f"Creating new chat session for ID: {session_id}")
        if not GEMINI_API_KEY:
             return None
        model = genai.GenerativeModel('gemini-1.5-flash')
        conversations[session_id] = model.start_chat(history=[])
    return conversations[session_id]

# The personality and instructions for our chatbot. This is the most important part!
BOT_PERSONA_PROMPT = """
אתה צ'אטבוט קולי בשם 'משיח צ'אט'. 
המטרה שלך היא לענות על שאלות בנושאי גאולה, משיח ותיקון עולם ברוח היהדות.
התשובות שלך חייבות להיות תמציתיות, ברורות ומתאימות להקראה בטלפון. הימנע מרשימות ארוכות או משפטים מורכבים.
עליך לדבר בעברית רהוטה, בטון אופטימי, מעורר השראה ומלא תקווה.
התבסס על מקורות יהודיים מגוונים, אך הגש את התשובות בשפה מודרנית ונגישה.
אל תציג את עצמך כבינה מלאכותית. דבר כקול המבטא את הציפייה לגאולה.
"""

# --- Flask Web Server Route ---
# This is the endpoint that Yemot HaMashiach will call.
@app.route('/api/chatbot', methods=['GET', 'POST'])
def handle_yemot_request():
    """
    Handles incoming requests from the Yemot HaMashiach IVR system.
    """
    try:
        # 1. Get data from the request
        form_data = request.form
        user_text = form_data.get('text', '').strip()
        caller_id = form_data.get('caller_id', 'default_user')

        logging.info(f"Received request from caller {caller_id} with text: '{user_text}'")

        if not GEMINI_API_KEY:
            logging.error("Gemini API key is not configured. Cannot process request.")
            yemot_response = "read=t-אירעה שגיאה במערכת, אנא נסו שנית במועד מאוחר יותר=,hangup"
            return Response(yemot_response, mimetype='text/plain')

        # 2. Get the specific chat session for this caller
        chat_session = get_chat_session(caller_id)
        if not chat_session:
             yemot_response = "read=t-אירעה שגיאה במערכת, אנא נסו שנית במועד מאוחר יותר=,hangup"
             return Response(yemot_response, mimetype='text/plain')

        # 3. Handle conversation flow (if no text, let IVR handle it)
        if not user_text:
            logging.info("No input text received. Assuming welcome message is handled by IVR.")
            return Response("", mimetype='text/plain')

        # 4. Construct the full prompt and send to Gemini
        full_prompt = f"{BOT_PERSONA_PROMPT}\n\nהמשתמש שאל: {user_text}"
        
        logging.info("Sending prompt to Gemini...")
        response = chat_session.send_message(full_prompt)
        gemini_answer = response.text
        logging.info(f"Received response from Gemini: '{gemini_answer}'")

        # 5. Format the response for Yemot HaMashiach
        # This format tells Yemot to read the text and then go back to the previous menu to listen again.
        yemot_response = f"go_to_folder=../play&api_result=t-{gemini_answer}"

    except Exception as e:
        logging.error(f"An error occurred: {e}", exc_info=True)
        yemot_response = "read=t-אירעה שגיאה, אנא נסו שנית=,hangup"
    
    logging.info(f"Sending response to Yemot: {yemot_response}")
    return Response(yemot_response, mimetype='text/plain')

# --- Run the Server ---
# This part is used for local testing. On Render, Gunicorn will be used as defined in the Start Command.
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8000, debug=True)
