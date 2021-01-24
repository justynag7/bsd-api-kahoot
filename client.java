package sample.net;

import java.io.EOFException;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.net.InetAddress;
import java.net.Socket;
import java.awt.BorderLayout;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.SwingUtilities;

public class client extends JFrame
{
    private JTextField enterField; // enters information from user
    private JLabel enterFieldLabel; //Label
    private JTextArea displayArea; // display information to user
    private ObjectOutputStream output; // output stream to server
    private ObjectInputStream input; // input stream from server
    private String message = ""; // message from server
    private String chatServer; // host server for this application
    private Socket client; // socket to communicate with server

    private String nickname;

    // initialize chatServer and set up GUI
    public client( String host,String nickname)
    {
        super( "Kahoot client" );
        this.nickname=nickname;
        chatServer = host; // set server to which this client connects

        enterFieldLabel = new JLabel("Enter filename to retrieve:");

        enterField = new JTextField(); // create enterField
        enterField.setEditable( true );
        enterField.addActionListener(
                new ActionListener()
                {
                    // send message to server
                    public void actionPerformed( ActionEvent event )
                    {
                        sendData( event.getActionCommand() );
                        enterField.setText( "" );
                    }
                }
        );

        JPanel panel = new JPanel(new BorderLayout());

        panel.add( enterFieldLabel, BorderLayout.WEST);
        panel.add( enterField, BorderLayout.CENTER );

        add(panel, BorderLayout.NORTH);

        displayArea = new JTextArea();
        add( new JScrollPane( displayArea ), BorderLayout.CENTER );

        setSize( 1200, 775 ); // set size of window
        setVisible( true ); // show window
    } // end Client constructor

    // connect to server and process messages from server
    public void runClient()
    {
        try // connect to server, get streams, process connection
        {
            connectToServer(); // create a Socket to make connection
            getStreams(); // get the input and output streams
            processConnection(); // process connection
        } // end try
        catch ( EOFException eofException )
        {
            displayMessage( "\nPlayer terminated connection" );
        } // end catch
        catch ( IOException ioException )
        {
            ioException.printStackTrace();
        } // end catch
        finally
        {
            closeConnection(); // close connection
        }
    }

    // connect to server
    private void connectToServer() throws IOException
    {
        displayMessage( "Attempting connection\n" );

        // create Socket to make connection to server
        client = new Socket( InetAddress.getByName( chatServer ), 12345 );

        // display connection information
        displayMessage( "Connected to: " +
                client.getInetAddress().getHostName() );
    }

    // get streams to send and receive data
    private void getStreams() throws IOException
    {
        // set up output stream for objects
        output = new ObjectOutputStream( client.getOutputStream() );
        output.flush(); // flush output buffer to send header information

        // set up input stream for objects
        input = new ObjectInputStream( client.getInputStream() );

        displayMessage( "\nGot I/O streams\n" );
    }

    // process connection with server
    private void processConnection() throws IOException
    {
        // enable enterField so client user can send messages
        setTextFieldEditable( true );

        do // process messages sent from server
        {
            try // read message and display it
            {
                message = ( String ) input.readObject(); // read new message
                displayMessage( "\n" + message ); // display message
            }
            catch ( ClassNotFoundException classNotFoundException )
            {
                displayMessage( "\nUnknown object type received" );
            }

        } while ( !message.equals( "SERVER>>> TERMINATE" ) );
    }
    // close streams and socket
    private void closeConnection()
    {
        displayMessage( "\nClosing connection" );
        setTextFieldEditable( false ); // disable enterField

        try
        {
            output.close(); // close output stream
            input.close(); // close input stream
            client.close(); // close socket
        }
        catch ( IOException ioException )
        {
            ioException.printStackTrace();
        }
    }

    // send message to server
    private void sendData( String message )
    {
        try // send object to server
        {
            output.writeObject( message );
            output.flush(); // flush data to output
            displayMessage( "\nCLIENT>>> " + message );
        }
        catch ( IOException ioException )
        {
            displayArea.append( "\nError writing object" );
        }
    }

    // manipulates displayArea in the event-dispatch thread
    private void displayMessage( final String messageToDisplay )
    {
        SwingUtilities.invokeLater(
                new Runnable()
                {
                    public void run() // updates displayArea
                    {
                        displayArea.append( messageToDisplay );
                    }
                }
        );
    }

    // manipulates enterField in the event-dispatch thread
    private void setTextFieldEditable( final boolean editable )
    {
        SwingUtilities.invokeLater(
                new Runnable()
                {
                    public void run() // sets enterField's editability
                    {
                        enterField.setEditable( editable );
                    }
                }
        );
    }
    public String getNickname() {
        return nickname;
    }

    public void setNickname(String nick) {
        this.nickname = nickname;
    }
}





