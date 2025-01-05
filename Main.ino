#include <SD.h> // Inclui a biblioteca SD

// Pino CS para o módulo SD
const int chipSelect = 10;

// Variável para armazenar o diretório atual
String currentDir = "/";

// Variable storage (simplified)
char variables[10][10]; // 10 variables, each with 10-character name
float values[10]; // Corresponding variable values
int varCount = 0;

// Program lines storage (simplified)
struct ProgramLine {
  int lineNumber;
  String command;
};
ProgramLine program[20]; // 20 lines of program
int lineCount = 0;

// Current line pointer for program execution
int currentLine = 0;

// Função para listar arquivos e diretórios no diretório atual
void handleDir() {
  File dir = SD.open(currentDir);
  if (!dir) {
    Serial.println(F("Failed to open directory."));
    return;
  }

  Serial.println(F("Directory listing:"));
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break; // Não há mais arquivos/diretórios
    }

    // Exibe o nome do arquivo/diretório
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println(F("/")); // Indica que é um diretório
    } else {
      // Exibe o tamanho do arquivo
      Serial.print(F(" - "));
      Serial.print(entry.size());
      Serial.println(F(" bytes"));
    }
    entry.close();
  }
  dir.close();
}

// Função para mudar de diretório
void handleCd(String input) {
  input = input.substring(3); // Remove "CD "
  input.trim(); // Remove espaços em branco

  if (input == "..") {
    // Voltar para o diretório pai
    int lastSlash = currentDir.lastIndexOf('/');
    if (lastSlash > 0) {
      currentDir = currentDir.substring(0, lastSlash);
    }
    Serial.println(F("Changed to parent directory."));
  } else {
    // Verificar se o diretório existe
    String newDir = currentDir + input + "/";
    if (SD.exists(newDir)) {
      currentDir = newDir;
      Serial.print(F("Changed to directory: "));
      Serial.println(currentDir);
    } else {
      Serial.println(F("Directory not found."));
    }
  }
}

// Função para exibir o diretório atual
void handlePwd() {
  Serial.print(F("Current directory: "));
  Serial.println(currentDir);
}

// Function to replace variables with their values (ignoring strings within quotes)
void replaceVariables(String &expr) {
  int startQuote = expr.indexOf('"'); // Find the first quote
  int endQuote = expr.lastIndexOf('"'); // Find the last quote

  // If there are no quotes, replace variables in the entire expression
  if (startQuote == -1 || endQuote == -1) {
    for (int i = 0; i < varCount; i++) {
      String varName = variables[i];
      String valueStr = String(values[i]);
      expr.replace(varName, valueStr);
    }
  } else {
    // Split the expression into parts: before quotes, within quotes, and after quotes
    String beforeQuotes = expr.substring(0, startQuote);
    String withinQuotes = expr.substring(startQuote, endQuote + 1);
    String afterQuotes = expr.substring(endQuote + 1);

    // Replace variables only in the parts outside the quotes
    for (int i = 0; i < varCount; i++) {
      String varName = variables[i];
      String valueStr = String(values[i]);
      beforeQuotes.replace(varName, valueStr);
      afterQuotes.replace(varName, valueStr);
    }

    // Rebuild the expression
    expr = beforeQuotes + withinQuotes + afterQuotes;
  }
}

// Function to evaluate the expression
// Function to evaluate the expression
float evaluateExpression(String expr) {
  // Remove todos os espaços da expressão
  expr.replace(" ", "");

  // Avalia subexpressões dentro de parênteses
  while (expr.indexOf('(') != -1) {
    int openParen = expr.lastIndexOf('('); // Encontra o último '('
    int closeParen = expr.indexOf(')', openParen); // Encontra o ')' correspondente

    if (closeParen == -1) {
      Serial.println(F("Error: Mismatched parentheses."));
      return 0;
    }

    // Extrai a subexpressão dentro dos parênteses
    String subExpr = expr.substring(openParen + 1, closeParen);

    // Avalia a subexpressão recursivamente
    float subResult = evaluateExpression(subExpr);

    // Substitui a subexpressão pelo resultado
    expr.replace("(" + subExpr + ")", String(subResult));
  }

  // Replace 'd' with '/' for division
  expr.replace('d', '/');

  // First pass: evaluate * and /
  while (expr.indexOf('*') != -1 || expr.indexOf('/') != -1) {
    int mulPos = expr.indexOf('*');
    int divPos = expr.indexOf('/');
    int opPos = -1;
    char op;
    if (mulPos != -1 && divPos != -1) {
      opPos = min(mulPos, divPos);
      op = expr.charAt(opPos);
    } else if (mulPos != -1) {
      opPos = mulPos;
      op = '*';
    } else if (divPos != -1) {
      opPos = divPos;
      op = '/';
    }
    if (opPos == -1) break;
    // Find the number before and after the operator
    int num1Start = opPos - 1;
    while (num1Start >= 0 && (isDigit(expr.charAt(num1Start)) || expr.charAt(num1Start) == '.')) num1Start--;
    num1Start++;
    int num1End = opPos;
    float num1 = expr.substring(num1Start, num1End).toFloat();
    int num2Start = opPos + 1;
    while (num2Start < expr.length() && (isDigit(expr.charAt(num2Start)) || expr.charAt(num2Start) == '.')) num2Start++;
    float num2 = expr.substring(opPos + 1, num2Start).toFloat();
    float result;
    if (op == '*') {
      result = num1 * num2;
    } else if (op == '/') {
      if (num2 == 0) {
        Serial.println(F("Error: Division by zero."));
        return 0;
      }
      result = num1 / num2;
    }
    // Replace the operation with the result
    String replacement = String(result);
    expr.replace(expr.substring(num1Start, num2Start), replacement);
  }
  // Second pass: evaluate + and -
  while (expr.indexOf('+') != -1 || expr.indexOf('-') != -1) {
    int addPos = expr.indexOf('+');
    int subPos = expr.indexOf('-');
    int opPos = -1;
    char op;
    if (addPos != -1 && subPos != -1) {
      opPos = min(addPos, subPos);
      op = expr.charAt(opPos);
    } else if (addPos != -1) {
      opPos = addPos;
      op = '+';
    } else if (subPos != -1) {
      opPos = subPos;
      op = '-';
    }
    if (opPos == -1) break;
    // Find the number before and after the operator
    int num1Start = opPos - 1;
    while (num1Start >= 0 && (isDigit(expr.charAt(num1Start)) || expr.charAt(num1Start) == '.')) num1Start--;
    num1Start++;
    int num1End = opPos;
    float num1 = expr.substring(num1Start, num1End).toFloat();
    int num2Start = opPos + 1;
    while (num2Start < expr.length() && (isDigit(expr.charAt(num2Start)) || expr.charAt(num2Start) == '.')) num2Start++;
    float num2 = expr.substring(opPos + 1, num2Start).toFloat();
    float result;
    if (op == '+') {
      result = num1 + num2;
    } else if (op == '-') {
      result = num1 - num2;
    }
    // Replace the operation with the result
    String replacement = String(result);
    expr.replace(expr.substring(num1Start, num2Start), replacement);
  }
  // The remaining expression should be a single number
  float finalResult = expr.toFloat();
  return finalResult;
}

void handlePrint(String input) {
  input = input.substring(6); // Remove "PRINT "
  input.trim(); // Remove espaços em branco no início e no fim

  // Split the input into parts separated by commas
  int commaPos = input.indexOf(',');
  while (commaPos != -1) {
    String part = input.substring(0, commaPos);
    part.trim();
    replaceVariables(part); // Substitui variáveis por seus valores (exceto dentro de aspas)

    // Verifica se há operações matemáticas
    if (part.indexOf('+') != -1 || part.indexOf('-') != -1 || part.indexOf('*') != -1 || part.indexOf('d') != -1) {
      float result = evaluateExpression(part); // Avalia a expressão
      Serial.print(result, 2); // Exibe o resultado com 2 casas decimais
    } else {
      // Se não houver operadores, exibe o valor diretamente
      Serial.print(part);
    }
    Serial.print(" "); // Adiciona um espaço entre os elementos
    input = input.substring(commaPos + 1);
    input.trim();
    commaPos = input.indexOf(',');
  }

  // Process the last part
  input.trim();
  replaceVariables(input); // Substitui variáveis por seus valores (exceto dentro de aspas)

  // Verifica se há operações matemáticas
  if (input.indexOf('+') != -1 || input.indexOf('-') != -1 || input.indexOf('*') != -1 || input.indexOf('d') != -1) {
    float result = evaluateExpression(input); // Avalia a expressão
    Serial.println(result, 2); // Exibe o resultado com 2 casas decimais
  } else {
    // Se não houver operadores, exibe o valor diretamente
    Serial.println(input);
  }
}

void handleLet(String input) {
  int equalsPos = input.indexOf('=');
  if (equalsPos == -1) {
    Serial.println(F("Invalid syntax for LET."));
    return;
  }
  String varName = input.substring(4, equalsPos);
  varName.trim();
  String valueStr = input.substring(equalsPos + 1);
  valueStr.trim();
  replaceVariables(valueStr); // Substitui variáveis na expressão

  // Avalia a expressão
  float value;
  if (valueStr.indexOf('+') != -1 || valueStr.indexOf('-') != -1 || valueStr.indexOf('*') != -1 || valueStr.indexOf('d') != -1) {
    value = evaluateExpression(valueStr);
  } else {
    value = valueStr.toFloat();
  }

  // Verifica se a variável já existe
  for (int i = 0; i < varCount; i++) {
    if (strcmp(variables[i], varName.c_str()) == 0) {
      values[i] = value; // Atualiza o valor da variável existente
      Serial.print(F("Variable "));
      Serial.print(varName);
      Serial.println(F(" updated."));
      return;
    }
  }

  // Se a variável não existe, cria uma nova
  if (varCount < 10) {
    strcpy(variables[varCount], varName.c_str());
    values[varCount] = value;
    varCount++;
    Serial.print(F("Variable "));
    Serial.print(varName);
    Serial.println(F(" created."));
  } else {
    Serial.println(F("Too many variables."));
  }
}

void handleGoto(String input) {
  input = input.substring(5);
  int targetLine = input.toInt();
  // Find the line number in program storage
  for (int i = 0; i < lineCount; i++) {
    if (program[i].lineNumber == targetLine) {
      currentLine = i;
      Serial.println(F("GOTO executed."));
      return;
    }
  }
  Serial.println(F("Line not found."));
}

void handleIf(String input) {
  // Simplified IF handling: IF condition THEN GOTO line
  int thenPos = input.indexOf("THEN");
  if (thenPos == -1) {
    Serial.println(F("Invalid syntax for IF."));
    return;
  }
  String condition = input.substring(2, thenPos);
  condition.trim();
  // For simplicity, check if condition is "var > value"
  int spacePos = condition.indexOf(' ');
  if (spacePos == -1) {
    Serial.println(F("Invalid condition."));
    return;
  }
  String varName = condition.substring(0, spacePos);
  String operatorStr = condition.substring(spacePos + 1, condition.indexOf(' ', spacePos + 1));
  String valueStr = condition.substring(condition.indexOf(' ', spacePos + 1) + 1);
  float value = valueStr.toFloat();
  // Find variable value
  float varValue = 0;
  for (int i = 0; i < varCount; i++) {
    if (strcmp(variables[i], varName.c_str()) == 0) {
      varValue = values[i];
      break;
    }
  }
  // Evaluate condition
  bool result = false;
  if (operatorStr == ">") {
    result = varValue > value;
  } else if (operatorStr == "<") {
    result = varValue < value;
  } else if (operatorStr == "=") {
    result = varValue == value;
  } else {
    Serial.println(F("Unsupported operator."));
    return;
  }
  // Execute GOTO if condition is true
  if (result) {
    int gotoLine = input.substring(input.indexOf("THEN") + 4).toInt();
    for (int i = 0; i < lineCount; i++) {
      if (program[i].lineNumber == gotoLine) {
        currentLine = i;
        Serial.println(F("IF condition met."));
        return;
      }
    }
    Serial.println(F("Line not found."));
  }
}

void runProgram() {
  currentLine = 0;
  while (currentLine < lineCount) {
    // Execute the current line
    String line = program[currentLine].command;
    if (line.startsWith("PRINT")) {
      handlePrint(line);
    } else if (line.startsWith("LET")) {
      handleLet(line);
    } else if (line.startsWith("GOTO")) {
      handleGoto(line);
    } else if (line.startsWith("IF")) {
      handleIf(line);
    } else if (line.startsWith("END")) {
      Serial.println(F("Program ended."));
      return; // Finaliza a execução do programa
    } else if (line.startsWith("REM")) {
      // Ignora comentários
    } else if (line.startsWith("PAUSE")) {
      handlePause(line);
    } else if (line.startsWith("RANDOM")) {
      handleRandom(line);
    } else if (line.startsWith("CLEAR")) {
      handleClear();
    } else {
      Serial.println(F("Unknown command in program."));
    }
    // Move to next line
    currentLine++;
  }
  Serial.println(F("Program end."));
}

void listProgram() {
  for (int i = 0; i < lineCount; i++) {
    Serial.print(program[i].lineNumber);
    Serial.print(F(" "));
    Serial.println(program[i].command);
  }
}

void newProgram() {
  lineCount = 0;
  varCount = 0;
  Serial.println(F("New program started."));
}

// Function to add a line to the program
void addProgramLine(String input) {
  int spacePos = input.indexOf(' ');
  if (spacePos == -1) {
    Serial.println(F("Invalid line format."));
    return;
  }
  int lineNumber = input.substring(0, spacePos).toInt();
  String command = input.substring(spacePos + 1);
  // Check if the line number already exists
  for (int i = 0; i < lineCount; i++) {
    if (program[i].lineNumber == lineNumber) {
      program[i].command = command; // Update existing line
      Serial.print(F("Line "));
      Serial.print(lineNumber);
      Serial.print(F(" updated: "));
      Serial.println(command);
      return;
    }
  }
  // Add new line
  if (lineCount < 20) {
    program[lineCount].lineNumber = lineNumber;
    program[lineCount].command = command;
    lineCount++;
    Serial.print(F("Line "));
    Serial.print(lineNumber);
    Serial.print(F(" added: "));
    Serial.println(command);
  } else {
    Serial.println(F("Too many lines."));
  }
}

// Function to edit a line in the program
void handleEdit(String input) {
  int spacePos = input.indexOf(' ');
  if (spacePos == -1) {
    Serial.println(F("Invalid syntax for EDIT."));
    return;
  }
  int lineNumber = input.substring(5, spacePos).toInt();
  String newCommand = input.substring(spacePos + 1);
  // Find the line number in program storage
  for (int i = 0; i < lineCount; i++) {
    if (program[i].lineNumber == lineNumber) {
      program[i].command = newCommand; // Update the line
      Serial.print(F("Line "));
      Serial.print(lineNumber);
      Serial.print(F(" edited: "));
      Serial.println(newCommand);
      return;
    }
  }
  Serial.println(F("Line not found."));
}

// Function to clear the screen
void handleCls() {
  for (int i = 0; i < 50; i++) {
    Serial.println();
  }
}

// Function to display help
void handleHelp() {
  Serial.println(F("Available commands:"));
  Serial.println(F("HELP - Display this help message"));
  Serial.println(F("PRINT <expression> - Print the result of an expression"));
  Serial.println(F("LET <var> = <expression> - Assign a value to a variable"));
  Serial.println(F("GOTO <line> - Jump to a specific line number"));
  Serial.println(F("IF <condition> THEN <line> - Conditional jump"));
  Serial.println(F("RUN - Execute the program"));
  Serial.println(F("LIST - List all program lines"));
  Serial.println(F("NEW - Clear the program"));
  Serial.println(F("CLS - Clear the screen"));
  Serial.println(F("EDIT <line> <command> - Edit a specific line"));
  Serial.println(F("END - End program execution"));
  Serial.println(F("REM <comment> - Add a comment"));
  Serial.println(F("PAUSE <time> - Pause execution for <time> milliseconds"));
  Serial.println(F("RENUM - Renumber program lines"));
  Serial.println(F("DELETE <line> - Delete a specific line"));
  Serial.println(F("RANDOM <var> - Generate a random number and assign to a variable"));
  Serial.println(F("CLEAR - Clear all variables and program lines"));
  Serial.println(F("RANDOM <Variable>, <MIN>, <MAX> - Create a variable that is randomized"));
  Serial.println(F("<line> <command> - Add a numbered line to the program"));
  Serial.println(F("LOAD <filename> - Load a program from the SD card"));
  Serial.println(F("SAVE <filename> - Save the program to the SD card"));
  Serial.println(F("DIR - List files and directories in the current directory"));
  Serial.println(F("CD <directory> - Change the current directory"));
  Serial.println(F("DIR <directory> - List the Directory"));
  Serial.println(F("PWD - Print the current directory"));
  Serial.println(F("RM - Delete files on the SD card."));
}

// Function to handle PAUSE command
void handlePause(String input) {
  input = input.substring(6); // Remove "PAUSE "
  input.trim(); // Remove espaços em branco
  int pauseTime = input.toInt(); // Converte o tempo de pausa para inteiro
  if (pauseTime > 0) {
    delay(pauseTime); // Pausa a execução pelo tempo especificado
    Serial.println(F("PAUSE executed."));
  } else {
    Serial.println(F("Invalid PAUSE time."));
  }
}

// Function to handle RENUM command
void handleRenum() {
  if (lineCount == 0) {
    Serial.println(F("No program lines to renumber."));
    return;
  }
  
  // Renumber lines starting from 10, incrementing by 10
  int newLineNumber = 10;
  for (int i = 0; i < lineCount; i++) {
    program[i].lineNumber = newLineNumber;
    newLineNumber += 10;
  }
  
  Serial.println(F("Program lines renumbered."));
}

// Function to handle DELETE command
void handleDelete(String input) {
  input = input.substring(7); // Remove "DELETE "
  input.trim(); // Remove espaços em branco
  int lineNumber = input.toInt(); // Converte o número da linha para inteiro
  
  // Procura a linha no programa
  for (int i = 0; i < lineCount; i++) {
    if (program[i].lineNumber == lineNumber) {
      // Remove a linha deslocando as linhas subsequentes
      for (int j = i; j < lineCount - 1; j++) {
        program[j] = program[j + 1];
      }
      lineCount--; // Reduz o contador de linhas
      Serial.print(F("Line "));
      Serial.print(lineNumber);
      Serial.println(F(" deleted."));
      return;
    }
  }
  
  Serial.println(F("Line not found."));
}

// Function to handle RANDOM command
void handleRandom(String input) {
  input = input.substring(7); // Remove "RANDOM "
  input.trim();

  // Verifica se o usuário especificou um intervalo
  int commaPos1 = input.indexOf(',');
  int commaPos2 = input.indexOf(',', commaPos1 + 1);

  if (commaPos1 == -1 || commaPos2 == -1) {
    Serial.println(F("Invalid syntax for RANDOM. Use: RANDOM VAR, MIN, MAX"));
    return;
  }

  String varName = input.substring(0, commaPos1);
  varName.trim();
  String minStr = input.substring(commaPos1 + 1, commaPos2);
  minStr.trim();
  String maxStr = input.substring(commaPos2 + 1);
  maxStr.trim();

  float minValue = minStr.toFloat();
  float maxValue = maxStr.toFloat();

  if (minValue >= maxValue) {
    Serial.println(F("Invalid range: MIN must be less than MAX."));
    return;
  }

  float randomValue = random(0, 1000000) / 1000000.0; // Gera um número com 6 casas decimais
  randomValue = minValue + randomValue * (maxValue - minValue); // Ajusta para o intervalo desejado

  // Verifica se a variável já existe
  for (int i = 0; i < varCount; i++) {
    if (strcmp(variables[i], varName.c_str()) == 0) {
      values[i] = randomValue; // Atualiza o valor da variável existente
      Serial.print(F("Variable "));
      Serial.print(varName);
      Serial.print(F(" updated to: "));
      Serial.println(randomValue, 6); // Exibe com 6 casas decimais
      return;
    }
  }

  // Se a variável não existe, cria uma nova
  if (varCount < 10) {
    strcpy(variables[varCount], varName.c_str());
    values[varCount] = randomValue;
    varCount++;
    Serial.print(F("Variable "));
    Serial.print(varName);
    Serial.print(F(" created with value: "));
    Serial.println(randomValue, 6); // Exibe com 6 casas decimais
  } else {
    Serial.println(F("Too many variables."));
  }
}

// Function to handle CLEAR command
void handleClear() {
  varCount = 0; // Reseta o contador de variáveis
  lineCount = 0; // Reseta o contador de linhas do programa
  Serial.println(F("Variables and program cleared."));
}

// Function to handle LOAD command
void handleLoad(String input) {
  input = input.substring(5); // Remove "LOAD "
  input.trim(); // Remove espaços em branco
  String filename = input + ".txt"; // Adiciona a extensão .txt

  File file = SD.open(filename);
  if (!file) {
    Serial.println(F("File not found."));
    return;
  }

  newProgram(); // Limpa o programa atual

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    addProgramLine(line); // Adiciona cada linha ao programa
  }

  file.close();
  Serial.println(F("Program loaded from SD card."));
}

// Função para apagar um arquivo do SD
void handleRm(String input) {
  input = input.substring(3); // Remove "RM "
  input.trim(); // Remove espaços em branco

  // Verifica se o nome do arquivo é válido
  if (input.length() == 0 || input.indexOf('/') != -1 || input.indexOf('\\') != -1) {
    Serial.println(F("Invalid filename."));
    return;
  }

  // Prepend current directory to the filename
  String fullPath = currentDir + "/" + input;

  // Verifica se o arquivo existe
  if (!SD.exists(fullPath)) {
    Serial.println(F("File not found."));
    return;
  }

  // Tenta remover o arquivo
  if (SD.remove(fullPath)) {
    Serial.print(F("File "));
    Serial.print(input);
    Serial.println(F(" deleted."));
  } else {
    Serial.println(F("Failed to delete file."));
  }
}

// Function to handle SAVE command
void handleSave(String input) {
  input = input.substring(5); // Remove "SAVE "
  input.trim(); // Remove espaços em branco
  String filename = input + ".txt"; // Adiciona a extensão .txt

  // Verifica se o nome do arquivo é válido
  if (filename.length() == 0 || filename.indexOf('/') != -1 || filename.indexOf('\\') != -1) {
    Serial.println(F("Invalid filename."));
    return;
  }

  // Prepend current directory to the filename
  String fullPath = currentDir + "/" + filename;

  // Verifica se o arquivo já existe e o remove
  if (SD.exists(fullPath)) {
    if (!SD.remove(fullPath)) {
      Serial.println(F("Failed to remove existing file."));
      return;
    }
  }

  // Tenta criar o arquivo com o caminho completo
  File file = SD.open(fullPath, FILE_WRITE);
  if (!file) {
    Serial.println(F("Error creating file. Check SD card."));
    return;
  }

  // Escreve cada linha do programa no arquivo
  for (int i = 0; i < lineCount; i++) {
    String line = String(program[i].lineNumber) + " " + program[i].command;
    if (!file.println(line)) {
      Serial.println(F("Error writing to file."));
      file.close();
      return;
    }
  }

  file.close();
  Serial.println(F("Program saved to SD card."));
}

// Setup function
void setup() {
  Serial.begin(9600); // Inicia a comunicação serial
  Serial.println(F("Arduino BASIC Emulator Ready!"));

  // Inicializa o cartão SD
  if (!SD.begin(10)) {
    Serial.println(F("SD card initialization failed!"));
    return;
  }
  Serial.println(F("SD card initialized."));
}

// Loop function
void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n'); // Lê a entrada do usuário
    input.trim(); // Remove espaços em branco no início e no fim
    if (input.startsWith("HELP")) {
      handleHelp();
    } else if (input.startsWith("PRINT")) {
      handlePrint(input);
    } else if (input.startsWith("LET")) {
      handleLet(input);
    } else if (input.startsWith("GOTO")) {
      handleGoto(input);
    } else if (input.startsWith("IF")) {
      handleIf(input);
    } else if (input.startsWith("RUN")) {
      runProgram();
    } else if (input.startsWith("LIST")) {
      listProgram();
    } else if (input.startsWith("NEW")) {
      newProgram();
    } else if (input.startsWith("CLS")) {
      handleCls();
    } else if (input.startsWith("EDIT")) {
      handleEdit(input);
    } else if (input.startsWith("END")) {
      Serial.println(F("Program ended."));
      return; // Finaliza a execução do programa
    } else if (input.startsWith("PAUSE")) {
      handlePause(input);
    } else if (input.startsWith("RENUM")) {
      handleRenum();
    } else if (input.startsWith("RM")) {
      handleRm(input);
    } else if (input.startsWith("DELETE")) {
      handleDelete(input);
    } else if (input.startsWith("RANDOM")) {
      handleRandom(input);
    } else if (input.startsWith("CLEAR")) {
      handleClear();
    } else if (input.startsWith("LOAD")) {
      handleLoad(input);
    } else if (input.startsWith("SAVE")) {
      handleSave(input);
    } else if (input.startsWith("CD")) {
      handleCd(input);
    } else if (input.startsWith("DIR")) {
      handleDir(); // Adiciona o comando DIR
    } else if (input.startsWith("PWD")) {
      handlePwd();
    } else if (isdigit(input.charAt(0))) {
      // If the input starts with a digit, assume it's a numbered line
      addProgramLine(input);
    } else {
      Serial.println(F("Unknown command. Type HELP for a list of commands."));
    }
  }
}
