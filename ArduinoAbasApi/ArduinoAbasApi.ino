#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>
#include <Console.h>
#include <Process.h>
#include <FileIO.h>

String key = "/root/.ssh/db_key";
String passw = " ... ";

YunServer server;



void prepareScripts(String key) {

  //Priprava scriptu exportu
  Console.println("Preparing first script ....\n");
  File script1 = FileSystem.open("/tmp/abasexport.sh", FILE_WRITE);
  script1.print("#!/bin/sh\n");
  script1.print("cd /u/abas/s3\n");
  script1.print("eval `sh denv.sh`\n");
  script1.print("edpinfosys.sh -m erp5 -p '"+passw+"' -n AQ.ARDUINO -s bstart -k - -f tprompt,tvalue > ~/datard\n");
  script1.print("exit\n");
  script1.close();

  //Priprava scriptu prenosu
  Console.println("Preparing sec. script .." + key + "..\n");
  File script2 = FileSystem.open("/tmp/abasexportstart.sh", FILE_WRITE);
  script2.print("rm -f /tmp/datard\n");

  // There is part of script responsible to transfer scrips to server and move data from server and cleanup
  //------
  script2.print("scp -i '" + key + "' /tmp/abasexport.sh root@zlin.amotiq.cz:\n");
  script2.print("ssh -i '" + key + "' root@zlin.amotiq.cz 'scp abasexport.sh root@debian6:'\n");
  script2.print("ssh -i '" + key + "' root@zlin.amotiq.cz 'ssh root@debian6 ./abasexport.sh'\n");
  script2.print("ssh -i '" + key + "' root@zlin.amotiq.cz 'scp root@debian6:datard ./'\n");
  script2.print("scp -i '" + key + "' root@zlin.amotiq.cz:datard /tmp/datard\n");
  // cleanup
  script2.print("ssh -i '" + key + "' root@zlin.amotiq.cz 'ssh root@debian6 rm -f abasexport.sh'\n");
  script2.print("ssh -i '" + key + "' root@zlin.amotiq.cz 'ssh root@debian6 rm -f datard'\n");
  script2.print("ssh -i '" + key + "' root@zlin.amotiq.cz 'rm -f abasexport.sh'\n");
  script2.print("ssh -i '" + key + "' root@zlin.amotiq.cz 'rm -f datard'\n");
  //------
  
  script2.close();

  // Make the script executable
  Console.println("Change attrib of an script ....\n");
  Process chmod;
  chmod.begin("chmod");
  chmod.addParameter("+x");
  chmod.addParameter("/tmp/abasexport.sh");
  chmod.addParameter("/tmp/abasexportstart.sh");
  chmod.run();

  //Setup crontab
  Console.println("Setup crontab ....\n");
  Process cron;
  cron.runShellCommand("crontab -ru root");
  cron.runShellCommand("echo '0 * * * * /tmp/abasexportstart.sh' | crontab -u root -");
//  cron.runShellCommand("/tmp/abasexportstart.sh");
Console.println(cron.runShellCommand("/tmp/abasexportstart.sh"));
}



String readValue(String param) {
  String prompt = "";
  String value = "";
  String hodnota = "";
  String msg = "";
  byte znak = 0;
  int idxfrst;
  int idxscnd;
  //Nacte soubor a ulozi potrebne hodnoty do promennych
  Console.println("Read data ....\n");

  msg = "Runtime:" + (String)(millis() / 1000) + " sec";
  File datard = FileSystem.open("/tmp/datard", FILE_READ);
  if (datard) {
    digitalWrite(13, HIGH);
    while (datard.available()) {
      znak = (byte)datard.read();
      if (znak != 10) {
        //Nacte cely radek
        hodnota += (char)znak;
      } else {
        Console.println("Line..." + hodnota + "\n");
        if (hodnota.startsWith(param)) {
          //Posklada zpravu
          idxfrst = hodnota.indexOf(";");
          idxscnd = hodnota.indexOf(";", idxfrst + 1);
          prompt = hodnota.substring(0, idxfrst);
          value  = hodnota.substring(idxfrst + 1, idxscnd);
          prompt.trim();
          value.trim();
          msg += "\n$" + prompt + ":" + value;
        }
        hodnota = "";
      }
    }
    datard.close();
    Console.println("Data ....\n" + msg + "\n");
  } else {
    digitalWrite(13, LOW);
  }
  return msg;
}



void checkStatus() {
  //Zmeni stav diody pokud neni datard - neni navazano spojeni s abasem
  if (FileSystem.exists("/tmp/datard")) {
    digitalWrite(13, HIGH);
  } else {
    digitalWrite(13, LOW);
  }
}



void setup() {
  Bridge.begin();
  Console.begin();
  // debug  while (!Console);
  FileSystem.begin();

  // initialize digital pin 13 as an output.
  pinMode(13, OUTPUT);

  // Listen for incoming connection only from localhost
  // (no one from the external network could connect)
  server.listenOnLocalhost();
  server.begin();

  // Prepare scripts and cron + first start scr.
  prepareScripts(key);

}



void loop() {
  YunClient client = server.accept();

  if (client) {
    String command = client.readString();
    command.trim();
    client.print(readValue(command));

    Console.println("Waiting...");

    client.stop();
  }

  //Provadi kazdou 1/20 sec
  checkStatus();
  delay(50);
}

