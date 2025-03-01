#include <SD.h>
#include <math.h>
#include <stdlib.h>
#include <MatrixMath.h>


// Pino CS para o módulo SD
const int chipSelect = 10;

// Variável para armazenar o diretório atual
String currentDir = "/";

// Armazenamento de variáveis (simplificado)
char variables[10][10]; // 10 variáveis, cada uma com nome de até 10 caracteres
float values[10]; // Valores correspondentes às variáveis
int varCount = 0;

// Variáveis globais para armazenar o grid
const int MAX_GRID_SIZE = 10; // Tamanho máximo do grid
char grid[MAX_GRID_SIZE][MAX_GRID_SIZE]; // Matriz para armazenar o grid
int gridWidth = 0; // Largura do grid
int gridHeight = 0; // Altura do grid

// Estrutura para armazenar grids
struct GridVariable {
  String name;
  int width;
  int height;
  char grid[MAX_GRID_SIZE][MAX_GRID_SIZE];
};

struct Grid {
  int width;
  int height;
  char grid[MAX_GRID_SIZE][MAX_GRID_SIZE];
};

Grid tempGrid; // Grid temporário para o comando SOR

// Array para armazenar variáveis de grid
GridVariable gridVariables[10];
int gridVarCount = 0;

// Estrutura para o hashmap (chave-valor)
struct HashMapEntry {
  String key;
  Grid value;
};

// Tamanho máximo do hashmap
const int HASHMAP_SIZE = 10;

// Array para armazenar as entradas do hashmap
HashMapEntry hashMap[HASHMAP_SIZE];
int hashMapCount = 0;

// Função para adicionar uma entrada ao hashmap
void addToHashMap(String key, Grid value) {
  if (hashMapCount < HASHMAP_SIZE) {
    hashMap[hashMapCount].key = key;
    hashMap[hashMapCount].value = value;
    hashMapCount++;
    Serial.print(F("Added to hashmap: "));
    Serial.println(key);
  } else {
    Serial.println(F("Hashmap is full!"));
  }
}

// Função para buscar uma entrada no hashmap
Grid* getFromHashMap(String key) {
  for (int i = 0; i < hashMapCount; i++) {
    if (hashMap[i].key == key) {
      return &hashMap[i].value;
    }
  }
  Serial.println(F("Key not found in hashmap!"));
  return nullptr;
}

struct AmalgVariable {
  String name;
  String type; // Pode ser "GRID", "VOID" ou "ENTER"
  int width;
  int height;
  char grid[MAX_GRID_SIZE][MAX_GRID_SIZE];
  AmalgVariable* next; // Ponteiro para o próximo grid na sequência
};

AmalgVariable* amalgVariables[10]; // Array de ponteiros para variáveis AMALG
int amalgVarCount = 0;

// Armazenamento de linhas do programa (simplificado)
struct ProgramLine {
  int lineNumber;
  String command;
};
ProgramLine program[20]; // 20 linhas de programa
int lineCount = 0;

// Ponteiro para a linha atual durante a execução do programa
int currentLine = 0;

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

// Função para rotacionar um grid em 90 graus
void rotateSingleGrid(char grid[MAX_GRID_SIZE][MAX_GRID_SIZE], int &width, int &height) {
  char rotatedGrid[MAX_GRID_SIZE][MAX_GRID_SIZE];

  // Rotaciona o grid por coluna
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      // A última coluna do grid original se torna a primeira linha do grid rotacionado
      rotatedGrid[j][height - 1 - i] = grid[i][width - 1 - j];
    }
  }

  // Atualiza as dimensões do grid
  int temp = width;
  width = height;
  height = temp;

  // Copia o grid rotacionado de volta para a matriz original
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      grid[i][j] = rotatedGrid[i][j];
    }
  }
}

// Função para rotacionar todo o AMALG
void handleRotateVar(String input) {
  input = input.substring(7); // Remove "ROTATE "
  input.trim();

  if (input.length() == 0) {
    Serial.println(F("Error: Missing AMALG variable name. Use: ROTATE <name>"));
    return;
  }

  String varName = input;

  // Procura a variável AMALG
  for (int i = 0; i < amalgVarCount; i++) {
    if (amalgVariables[i]->name == varName) {
      AmalgVariable* current = amalgVariables[i];

      // Rotaciona cada grid no AMALG
      while (current != nullptr) {
        if (current->type.equalsIgnoreCase("GRID") || current->type.equalsIgnoreCase("VOID")) {
          rotateSingleGrid(current->grid, current->width, current->height);
        }
        current = current->next;
      }

      Serial.print(F("AMALG variable "));
      Serial.print(varName);
      Serial.println(F(" rotated 90 degrees."));

      // Exibe o AMALG após a rotação
      current = amalgVariables[i];
      while (current != nullptr) {
        if (current->type.equalsIgnoreCase("ENTER")) {
          Serial.println(); // Apenas uma nova linha para ENTER
        } else {
          for (int i = 0; i < current->height; i++) {
            for (int j = 0; j < current->width; j++) {
              Serial.print('[');
              Serial.print(current->grid[i][j]);
              Serial.print(']');
            }
            if (i < current->height - 1) {
              Serial.println(); // Nova linha apenas entre as linhas do grid
            }
          }
        }
        current = current->next;
      }

      return;
    }
  }

  Serial.println(F("AMALG variable not found."));
}

// Função para lidar com o comando AMALG
void handleAmalg(String input) {
  input = input.substring(6); // Remove "AMALG "
  input.trim();

  // Divide a entrada em partes separadas por ';'
  String commands[10]; // Armazena até 10 comandos
  int commandCount = 0;
  int startIndex = 0;

  for (int i = 0; i < input.length(); i++) {
    if (input.charAt(i) == ';') {
      commands[commandCount] = input.substring(startIndex, i);
      commands[commandCount].trim();
      commandCount++;
      startIndex = i + 1;
    }
  }

  // Adiciona o último comando (se houver)
  if (startIndex < input.length()) {
    commands[commandCount] = input.substring(startIndex);
    commands[commandCount].trim();
    commandCount++;
  }

  // Verifica se há comandos para processar
  if (commandCount == 0) {
    Serial.println(F("Error: No commands provided for AMALG."));
    return;
  }

  // Processa o primeiro comando para criar a variável AMALG
  String firstCommand = commands[0];
  int spacePos1 = firstCommand.indexOf(' ');
  int spacePos2 = firstCommand.indexOf(' ', spacePos1 + 1);
  int spacePos3 = firstCommand.indexOf(' ', spacePos2 + 1);

  if (spacePos1 == -1 || spacePos2 == -1 || spacePos3 == -1) {
    Serial.println(F("Invalid syntax for AMALG. Use: AMALG <name> <type> <width> <height>"));
    return;
  }

  String varName = firstCommand.substring(0, spacePos1);
  String type = firstCommand.substring(spacePos1 + 1, spacePos2);
  String widthStr = firstCommand.substring(spacePos2 + 1, spacePos3);
  String heightStr = firstCommand.substring(spacePos3 + 1);

  int width = widthStr.toInt();
  int height = heightStr.toInt();

  if (width <= 0 || height <= 0 || width > MAX_GRID_SIZE || height > MAX_GRID_SIZE) {
    Serial.println(F("Error: Invalid grid dimensions."));
    return;
  }

  if (amalgVarCount < 10) {
    // Cria a variável AMALG principal
    AmalgVariable* mainVar = new AmalgVariable();
    mainVar->name = varName;
    mainVar->type = type;
    mainVar->width = width;
    mainVar->height = height;
    mainVar->next = nullptr;

    // Preenche o grid com base no tipo
    if (type.equalsIgnoreCase("GRID")) {
      for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
          mainVar->grid[i][j] = '#';
        }
      }
    } else if (type.equalsIgnoreCase("VOID")) {
      for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
          mainVar->grid[i][j] = ' ';
        }
      }
    } else if (type.equalsIgnoreCase("ENTER")) {
      Serial.println();
    } else {
      Serial.println(F("Error: Invalid type. Use GRID, VOID, or ENTER."));
      delete mainVar;
      return;
    }

    // Processa os comandos subsequentes
    AmalgVariable* currentVar = mainVar;
    for (int i = 1; i < commandCount; i++) {
      String command = commands[i];
      command.trim();

      if (command.equalsIgnoreCase("ENTER")) {
        // Cria um novo grid para o ENTER
        AmalgVariable* newVar = new AmalgVariable();
        newVar->type = "ENTER";
        newVar->width = 1;
        newVar->height = 1;
        newVar->grid[0][0] = '\n';
        newVar->next = nullptr;

        currentVar->next = newVar;
        currentVar = newVar;
      } else if (command.startsWith("VOID")) {
        // Processa o comando VOID
        int spacePos1 = command.indexOf(' ');
        int spacePos2 = command.indexOf(' ', spacePos1 + 1);

        if (spacePos1 == -1 || spacePos2 == -1) {
          Serial.println(F("Invalid syntax for VOID. Use: VOID <width> <height>"));
          continue;
        }

        String widthStr = command.substring(spacePos1 + 1, spacePos2);
        String heightStr = command.substring(spacePos2 + 1);

        int width = widthStr.toInt();
        int height = heightStr.toInt();

        if (width <= 0 || height <= 0 || width > MAX_GRID_SIZE || height > MAX_GRID_SIZE) {
          Serial.println(F("Error: Invalid grid dimensions for VOID."));
          continue;
        }

        // Cria um novo grid para o VOID
        AmalgVariable* newVar = new AmalgVariable();
        newVar->type = "VOID";
        newVar->width = width;
        newVar->height = height;

        for (int i = 0; i < height; i++) {
          for (int j = 0; j < width; j++) {
            newVar->grid[i][j] = ' ';
          }
        }

        newVar->next = nullptr;
        currentVar->next = newVar;
        currentVar = newVar;
      } else if (command.startsWith("GRID")) {
        // Processa o comando GRID
        int spacePos1 = command.indexOf(' ');
        int spacePos2 = command.indexOf(' ', spacePos1 + 1);

        if (spacePos1 == -1 || spacePos2 == -1) {
          Serial.println(F("Invalid syntax for GRID. Use: GRID <width> <height>"));
          continue;
        }

        String widthStr = command.substring(spacePos1 + 1, spacePos2);
        String heightStr = command.substring(spacePos2 + 1);

        int width = widthStr.toInt();
        int height = heightStr.toInt();

        if (width <= 0 || height <= 0 || width > MAX_GRID_SIZE || height > MAX_GRID_SIZE) {
          Serial.println(F("Error: Invalid grid dimensions for GRID."));
          continue;
        }

        // Cria um novo grid para o GRID
        AmalgVariable* newVar = new AmalgVariable();
        newVar->type = "GRID";
        newVar->width = width;
        newVar->height = height;

        for (int i = 0; i < height; i++) {
          for (int j = 0; j < width; j++) {
            newVar->grid[i][j] = '#';
          }
        }

        newVar->next = nullptr;
        currentVar->next = newVar;
        currentVar = newVar;
      } else {
        Serial.print(F("Unknown command in AMALG sequence: "));
        Serial.println(command);
      }
    }

    // Armazena a variável AMALG no array
    amalgVariables[amalgVarCount] = mainVar;
    amalgVarCount++;

    Serial.print(F("AMALG variable "));
    Serial.print(varName);
    Serial.println(F(" created with multiple grids."));

    // Exibe o AMALG
    AmalgVariable* current = mainVar;
    while (current != nullptr) {
      if (current->type.equalsIgnoreCase("ENTER")) {
        Serial.println(); // Apenas uma nova linha para ENTER
      } else {
        for (int i = 0; i < current->height; i++) {
          for (int j = 0; j < current->width; j++) {
            Serial.print('[');
            Serial.print(current->grid[i][j]);
            Serial.print(']');
          }
          if (i < current->height - 1) {
            Serial.println(); // Nova linha apenas entre as linhas do grid
          }
        }
      }
      current = current->next;
    }
  } else {
    Serial.println(F("Too many AMALG variables."));
  }
}

// Função para lidar com o comando VARGRID
void handleVarGrid(String input) {
  input = input.substring(8); // Remove "VARGRID "
  input.trim();

  int spacePos1 = input.indexOf(' ');
  int spacePos2 = input.indexOf(' ', spacePos1 + 1);

  if (spacePos1 == -1 || spacePos2 == -1) {
    Serial.println(F("Invalid syntax for VARGRID. Use: VARGRID <name> <width> <height>"));
    return;
  }

  String varName = input.substring(0, spacePos1);
  String widthStr = input.substring(spacePos1 + 1, spacePos2);
  String heightStr = input.substring(spacePos2 + 1);

  int width = widthStr.toInt();
  int height = heightStr.toInt();

  if (width <= 0 || height <= 0 || width > MAX_GRID_SIZE || height > MAX_GRID_SIZE) {
    Serial.println(F("Error: Invalid grid dimensions."));
    return;
  }

  if (gridVarCount < 10) {
    gridVariables[gridVarCount].name = varName;
    gridVariables[gridVarCount].width = width;
    gridVariables[gridVarCount].height = height;

    // Preenche o grid com o padrão '#'
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++) {
        gridVariables[gridVarCount].grid[i][j] = '#';
      }
    }

    gridVarCount++;
    Serial.print(F("Grid variable "));
    Serial.print(varName);
    Serial.print(F(" created with dimensions "));
    Serial.print(width);
    Serial.print(F("x"));
    Serial.println(height);
  } else {
    Serial.println(F("Too many grid variables."));
  }
}

// Função para lidar com o comando VARVOID
void handleVarVoid(String input) {
  input = input.substring(8); // Remove "VARVOID "
  input.trim();

  int spacePos1 = input.indexOf(' ');
  int spacePos2 = input.indexOf(' ', spacePos1 + 1);

  if (spacePos1 == -1 || spacePos2 == -1) {
    Serial.println(F("Invalid syntax for VARVOID. Use: VARVOID <name> <width> <height>"));
    return;
  }

  String varName = input.substring(0, spacePos1);
  String widthStr = input.substring(spacePos1 + 1, spacePos2);
  String heightStr = input.substring(spacePos2 + 1);

  int width = widthStr.toInt();
  int height = heightStr.toInt();

  if (width <= 0 || height <= 0 || width > MAX_GRID_SIZE || height > MAX_GRID_SIZE) {
    Serial.println(F("Error: Invalid grid dimensions."));
    return;
  }

  if (gridVarCount < 10) {
    gridVariables[gridVarCount].name = varName;
    gridVariables[gridVarCount].width = width;
    gridVariables[gridVarCount].height = height;

    // Preenche o grid com espaços em branco
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++) {
        gridVariables[gridVarCount].grid[i][j] = ' ';
      }
    }

    gridVarCount++;
    Serial.print(F("Void grid variable "));
    Serial.print(varName);
    Serial.print(F(" created with dimensions "));
    Serial.print(width);
    Serial.print(F("x"));
    Serial.println(height);
  } else {
    Serial.println(F("Too many grid variables."));
  }
}

// Função para lidar com o comando VARENTER
void handleVarEnter(String input) {
  input = input.substring(9); // Remove "VARENTER "
  input.trim();

  if (input.length() == 0) {
    Serial.println(F("Invalid syntax for VARENTER. Use: VARENTER <name>"));
    return;
  }

  String varName = input;

  // Procura a variável de grid
  for (int i = 0; i < gridVarCount; i++) {
    if (gridVariables[i].name == varName) {
      // Exibe o grid sem nova linha no final
      for (int j = 0; j < gridVariables[i].height; j++) {
        for (int k = 0; k < gridVariables[i].width; k++) {
          Serial.print('[');
          Serial.print(gridVariables[i].grid[j][k]);
          Serial.print(']');
        }
        if (j < gridVariables[i].height - 1) {
          Serial.println(); // Nova linha apenas entre as linhas do grid
        }
      }
      return;
    }
  }

  Serial.println(F("Grid variable not found."));
}

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

// Função para lidar com o comando LOG
void handleLog(String input) {
  input = input.substring(4); // Remove "LOG "
  input.trim(); // Remove espaços em branco

  // Verifica se a entrada está vazia
  if (input.length() == 0) {
    Serial.println(F("Error: Missing argument for LOG."));
    return;
  }

  // Substitui variáveis por seus valores
  replaceVariables(input);
  float value = evaluateExpression(input);

  // Verifica se o valor é válido (deve ser maior que 0)
  if (value <= 0) {
    Serial.println(F("Error: LOG argument must be greater than 0."));
    return;
  }

  // Calcula o logaritmo na base 10
  float result = log10(value);

  // Exibe o resultado
  Serial.print(F("LOG("));
  Serial.print(value, 6);
  Serial.print(F(") = "));
  Serial.println(result, 6);
}

// Função para substituir variáveis por seus valores
void replaceVariables(String &expr) {
  int startQuote = expr.indexOf('"');
  int endQuote = expr.lastIndexOf('"');

  if (startQuote == -1 || endQuote == -1) {
    for (int i = 0; i < varCount; i++) {
      String varName = variables[i];
      String valueStr = String(values[i]);
      expr.replace(varName, valueStr);
    }
  } else {
    String beforeQuotes = expr.substring(0, startQuote);
    String withinQuotes = expr.substring(startQuote, endQuote + 1);
    String afterQuotes = expr.substring(endQuote + 1);

    for (int i = 0; i < varCount; i++) {
      String varName = variables[i];
      String valueStr = String(values[i]);
      beforeQuotes.replace(varName, valueStr);
      afterQuotes.replace(varName, valueStr);
    }

    expr = beforeQuotes + withinQuotes + afterQuotes;
  }
}

// Função para avaliar a expressão
float evaluateExpression(String expr) {
    expr.replace(" ", "");
    
    // Primeiro substituir todas as variáveis
    replaceVariables(expr);
    
    // Depois verificar funções especiais como SQR()
    if (expr.startsWith("SQR(") && expr.endsWith(")")) {
        String arg = expr.substring(4, expr.length() - 1);
        float value = evaluateExpression(arg);
        return sqrt(value);
    }

  if (expr.length() == 0) {
    Serial.println(F("Error: Empty expression."));
    return 0;
  }

  int startQuote = expr.indexOf('"');
  int endQuote = expr.lastIndexOf('"');

  if (startQuote != -1 && endQuote != -1) {
    String beforeQuotes = expr.substring(0, startQuote);
    String withinQuotes = expr.substring(startQuote, endQuote + 1);
    String afterQuotes = expr.substring(endQuote + 1);

    float beforeResult = evaluateExpression(beforeQuotes);
    float afterResult = evaluateExpression(afterQuotes);

    return beforeResult + afterResult;
  }

  while (expr.indexOf('(') != -1) {
    int openParen = expr.lastIndexOf('(');
    int closeParen = expr.indexOf(')', openParen);

    if (closeParen == -1) {
      Serial.println(F("Error: Mismatched parentheses."));
      return 0;
    }

    String subExpr = expr.substring(openParen + 1, closeParen);
    float subResult = evaluateExpression(subExpr);
    expr.replace("(" + subExpr + ")", String(subResult));
  }

  expr.replace('d', '/');

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

    int num1Start = opPos - 1;
    while (num1Start >= 0 && (isDigit(expr.charAt(num1Start)) || expr.charAt(num1Start) == '.')) num1Start--;
    num1Start++;
    int num1End = opPos;
    float num1 = expr.substring(num1Start, num1End).toFloat();

    int num2Start = opPos + 1;
    while (num2Start < expr.length() && (isDigit(expr.charAt(num2Start)) || expr.charAt(num2Start) == '.')) num2Start++;
    float num2 = expr.substring(opPos + 1, num2Start).toFloat();

    if (isnan(num1) || isnan(num2)) {
      Serial.println(F("Error: Invalid operands."));
      return 0;
    }

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

    expr.replace(expr.substring(num1Start, num2Start), String(result));
  }

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

    int num1Start = opPos - 1;
    while (num1Start >= 0 && (isDigit(expr.charAt(num1Start)) || expr.charAt(num1Start) == '.')) num1Start--;
    num1Start++;
    int num1End = opPos;
    float num1 = expr.substring(num1Start, num1End).toFloat();

    int num2Start = opPos + 1;
    while (num2Start < expr.length() && (isDigit(expr.charAt(num2Start)) || expr.charAt(num2Start) == '.')) num2Start++;
    float num2 = expr.substring(opPos + 1, num2Start).toFloat();

    if (isnan(num1) || isnan(num2)) {
      Serial.println(F("Error: Invalid operands."));
      return 0;
    }

    float result;
    if (op == '+') {
      result = num1 + num2;
    } else if (op == '-') {
      result = num1 - num2;
    }

    expr.replace(expr.substring(num1Start, num2Start), String(result));
  }

  return expr.toFloat();
}

// Função para avaliar uma condição
bool evaluateCondition(String condition) {
  if (condition.indexOf(">") != -1) {
    int pos = condition.indexOf(">");
    String left = condition.substring(0, pos);
    String right = condition.substring(pos + 1);
    left.trim();
    right.trim();

    float leftValue = evaluateExpression(left);
    float rightValue = evaluateExpression(right);

    return leftValue > rightValue;
  } else if (condition.indexOf("<") != -1) {
    int pos = condition.indexOf("<");
    String left = condition.substring(0, pos);
    String right = condition.substring(pos + 1);
    left.trim();
    right.trim();

    float leftValue = evaluateExpression(left);
    float rightValue = evaluateExpression(right);

    return leftValue < rightValue;
  } else if (condition.indexOf("=") != -1) {
    int pos = condition.indexOf("=");
    String left = condition.substring(0, pos);
    String right = condition.substring(pos + 1);
    left.trim();
    right.trim();

    float leftValue = evaluateExpression(left);
    float rightValue = evaluateExpression(right);

    return leftValue == rightValue;
  } else {
    Serial.println(F("Unsupported condition operator."));
    return false;
  }
}

// Função para lidar com o comando PRINT
void handlePrint(String input) {
  input = input.substring(6); // Remove "PRINT "
  input.trim();

  String parts[10];
  int partCount = 0;

  bool insideQuotes = false;
  int startIndex = 0;

  for (int i = 0; i < input.length(); i++) {
    char currentChar = input.charAt(i);

    if (currentChar == '"') {
      insideQuotes = !insideQuotes;
    }

    if (currentChar == ';' && !insideQuotes) {
      parts[partCount] = input.substring(startIndex, i);
      parts[partCount].trim();
      partCount++;
      startIndex = i + 1;
    }
  }

  if (startIndex < input.length()) {
    parts[partCount] = input.substring(startIndex);
    parts[partCount].trim();
    partCount++;
  }

  for (int i = 0; i < partCount; i++) {
    String part = parts[i];

    if (part.startsWith("\"") && part.endsWith("\"")) {
      Serial.print(part.substring(1, part.length() - 1));
    } else {
      replaceVariables(part);
      float result = evaluateExpression(part);
      if (!isnan(result)) {
        Serial.print(result, 2);
      } else {
        Serial.print("NaN");
      }
    }

    if (i < partCount - 1) {
      Serial.print(" ");
    }
  }

  Serial.println();
}

// Função para lidar com o comando LET
void handleLet(String input) {
  int equalsPos = input.indexOf('=');
  if (equalsPos == -1) {
    Serial.println(F("Invalid syntax for LET."));
    return;
  }

  String varName = input.substring(4, equalsPos);
  varName.trim();

  String expr = input.substring(equalsPos + 1);
  expr.trim();

  replaceVariables(expr);
  float value = evaluateExpression(expr);

  for (int i = 0; i < varCount; i++) {
    if (strcmp(variables[i], varName.c_str()) == 0) {
      values[i] = value;
      Serial.print(F("Variable "));
      Serial.print(varName);
      Serial.print(F(" updated to: "));
      Serial.println(value, 6);
      return;
    }
  }

  if (varCount < 10) {
    strcpy(variables[varCount], varName.c_str());
    values[varCount] = value;
    varCount++;
    Serial.print(F("Variable "));
    Serial.print(varName);
    Serial.print(F(" created with value: "));
    Serial.println(value, 6);
  } else {
    Serial.println(F("Too many variables."));
  }
}

// Função para lidar com o comando GOTO
void handleGoto(String input) {
  input = input.substring(5);
  int targetLine = input.toInt();

  for (int i = 0; i < lineCount; i++) {
    if (program[i].lineNumber == targetLine) {
      currentLine = i;
      Serial.println(F("GOTO executed."));
      return;
    }
  }

  Serial.println(F("Line not found."));
}

// Função para lidar com o comando IF
void handleIf(String input) {
  int thenPos = input.indexOf("THEN");
  if (thenPos == -1) {
    Serial.println(F("Invalid syntax for IF."));
    return;
  }

  String condition = input.substring(2, thenPos);
  condition.trim();

  String action = input.substring(thenPos + 4);
  action.trim();

  bool result = evaluateCondition(condition);

  if (result) {
    if (action.startsWith("GOTO")) {
      int gotoLine = action.substring(5).toInt();
      for (int i = 0; i < lineCount; i++) {
        if (program[i].lineNumber == gotoLine) {
          currentLine = i; // Adjusted to set currentLine correctly
          Serial.print(F("IF condition met. Jumping to line "));
          Serial.println(gotoLine);
          return;
        }
      }
      Serial.println(F("Line not found."));
    } else if (action.startsWith("PRINT")) {
      handlePrint(action);
    } else if (action.startsWith("LET")) {
      handleLet(action);
    } else if (action.startsWith("ASK")) {
      handleAsk(action);
    } else if (action.startsWith("SQRT")) {
      handleSqrt(action);
    } else {
      Serial.println(F("Unsupported action after THEN."));
    }
  } else {
    Serial.println(F("IF condition not met."));
  }
}

// Função para lidar com o comando SVARG
void handleSvarg(String input) {
  input = input.substring(6); // Remove "SVARG "
  input.trim(); // Remove espaços em branco

  if (input.length() == 0) {
    Serial.println(F("Error: Missing grid variable name. Use: SVARG <name>"));
    return;
  }

  String varName = input;

  // Procura a variável de grid
  for (int i = 0; i < gridVarCount; i++) {
    if (gridVariables[i].name == varName) {
      // Exibe o grid
      for (int j = 0; j < gridVariables[i].height; j++) {
        for (int k = 0; k < gridVariables[i].width; k++) {
          Serial.print('[');
          Serial.print(gridVariables[i].grid[j][k]);
          Serial.print(']');
        }
        Serial.println();
      }
      return;
    }
  }

  Serial.println(F("Grid variable not found."));
}

// Função para lidar com o comando SOR
void handleSor(String input) {
  input = input.substring(4); // Remove "SOR "
  input.trim(); // Remove espaços em branco

  // Inicializa o grid temporário com espaços em branco
  tempGrid.width = MAX_GRID_SIZE; // Define a largura máxima
  tempGrid.height = MAX_GRID_SIZE; // Define a altura máxima
  for (int i = 0; i < MAX_GRID_SIZE; i++) {
    for (int j = 0; j < MAX_GRID_SIZE; j++) {
      tempGrid.grid[i][j] = ' ';
    }
  }

  // Extrai as coordenadas iniciais
  int spacePos1 = input.indexOf(' ');
  int spacePos2 = input.indexOf(' ', spacePos1 + 1);
  int parenPos = input.indexOf('(');

  if (spacePos1 == -1 || spacePos2 == -1 || parenPos == -1) {
    Serial.println(F("Invalid syntax for SOR. Use: SOR <row> <col> (1 1 = \"A\"; 1 2 = \"B\"; ...)"));
    return;
  }

  String rowStr = input.substring(0, spacePos1);
  String colStr = input.substring(spacePos1 + 1, spacePos2);
  String assignments = input.substring(parenPos + 1, input.length() - 1);

  int startRow = rowStr.toInt() - 1; // Subtrai 1 para indexação baseada em 0
  int startCol = colStr.toInt() - 1; // Subtrai 1 para indexação baseada em 0
  int numColumns = colStr.toInt();   // Número de colunas especificado no comando

  // Divide as atribuições em partes separadas por ';'
  String parts[10]; // Armazena até 10 atribuições
  int partCount = 0;
  int startIndex = 0;

  for (int i = 0; i < assignments.length(); i++) {
    if (assignments.charAt(i) == ';') {
      parts[partCount] = assignments.substring(startIndex, i);
      parts[partCount].trim();
      partCount++;
      startIndex = i + 1;
    }
  }

  // Adiciona a última parte (se houver)
  if (startIndex < assignments.length()) {
    parts[partCount] = assignments.substring(startIndex);
    parts[partCount].trim();
    partCount++;
  }

  // Verifica se o número de colunas corresponde ao número de atribuições
  if (numColumns < partCount) {
    Serial.print(F("Err: "));
  } else if (numColumns > partCount) {
    Serial.print(F("Overflow: "));
  }

  // Processa cada atribuição
  for (int i = 0; i < partCount; i++) {
    String part = parts[i];
    part.trim();

    // Verifica o formato da atribuição: "linha coluna = valor"
    int spacePos1 = part.indexOf(' ');
    int equalsPos = part.indexOf('=');
    int spacePos2 = part.indexOf(' ', spacePos1 + 1);

    if (spacePos1 == -1 || equalsPos == -1 || spacePos2 == -1) {
      Serial.println(F("Invalid syntax for SOR. Use: SOR <row> <col> (1 1 = \"A\"; 1 2 = \"B\"; ...)"));
      return;
    }

    // Extrai linha, coluna e valor
    String rowStr = part.substring(0, spacePos1);
    String colStr = part.substring(spacePos1 + 1, equalsPos);
    String valueStr = part.substring(equalsPos + 1);
    rowStr.trim();
    colStr.trim();
    valueStr.trim();

    // Converte para inteiros
    int row = rowStr.toInt() - 1; // Subtrai 1 para indexação baseada em 0
    int col = colStr.toInt() - 1; // Subtrai 1 para indexação baseada em 0

    // Verifica se as coordenadas são válidas
    if (startRow + row < 0 || startRow + row >= MAX_GRID_SIZE || startCol + col < 0 || startCol + col >= MAX_GRID_SIZE) {
      Serial.println(F("Error: Invalid row or column."));
      continue; // Ignora esta atribuição e continua com as próximas
    }

    // Atribui o valor ao grid temporário
    if (valueStr.startsWith("\"") && valueStr.endsWith("\"")) {
      // Remove as aspas e atribui o primeiro caractere
      valueStr = valueStr.substring(1, valueStr.length() - 1);
      tempGrid.grid[startRow + row][startCol + col] = valueStr.charAt(0);
    } else {
      // Atribui o valor diretamente (para números ou caracteres sem aspas)
      tempGrid.grid[startRow + row][startCol + col] = valueStr.charAt(0);
    }
  }

  // Exibe a linha preenchida
  for (int j = 0; j < numColumns; j++) {
    Serial.print('[');
    if (j < partCount) {
      Serial.print(tempGrid.grid[startRow][startCol + j]);
    } else {
      Serial.print(' '); // Preenche colunas extras com espaço em branco
    }
    Serial.print(']');
  }
  Serial.println();
}

// Função para lidar com o comando SQRT
void handleSqrt(String input) {
  input = input.substring(5); // Remove "SQRT "
  input.trim();

  if (input.length() == 0) {
    Serial.println(F("Error: Missing argument for SQRT."));
    return;
  }

  replaceVariables(input);
  float value = evaluateExpression(input);

  if (value < 0) {
    Serial.println(F("Error: SQRT argument must be non-negative."));
    return;
  }

  float result = sqrt(value);

  Serial.print(F("SQRT("));
  Serial.print(value, 6);
  Serial.print(F(") = "));
  Serial.println(result, 6);
}

// Função para executar o programa
void runProgram() {
  currentLine = 0;
  while (currentLine < lineCount) {
    String line = program[currentLine].command;
    if (line.startsWith("VARGRID")) {
      handleVarGrid(line);
    } else if (line.startsWith("VARVOID")) {
      handleVarVoid(line);
    } else if (line.startsWith("VARENTER")) {
      handleVarEnter(line);
    } else if (line.startsWith("VOID")) {
      handleVoid(line);
    } else if (line.equalsIgnoreCase("ROTATE")) {
      handleRotate();
    } else if (line.startsWith("ROTATE ")) {
      handleRotateVar(line);
    } else if (line.startsWith("GRID")) {
      handleGrid(line);
    } else if (line.startsWith("PRINT")) {
      handlePrint(line);
    } else if (line.startsWith("LET")) {
      handleLet(line);
    } else if (line.startsWith("GOTO")) {
      handleGoto(line);
    } else if (line.startsWith("IF")) {
      handleIf(line);
    } else if (line.startsWith("END")) {
      Serial.println(F("Program ended."));
      return;
    } else if (line.startsWith("REM")) {
      // Ignora comentários
    } else if (line.startsWith("PAUSE")) {
      handlePause(line);
    } else if (line.startsWith("RANDOM")) {
      handleRandom(line);
    } else if (line.startsWith("CLEAR")) {
      handleClear();
    } else if (line.startsWith("ASK")) {
      handleAsk(line);
    } else if (line.startsWith("LOG")) {
      handleLog(line);
    } else if (line.startsWith("SVARG")) { 
      handleSvarg(line);
    } else if (line.startsWith("SQRT")) {
      handleSqrt(line);
    } else if (line.startsWith("ENTER")) {
      handleEnter();
    } else if (line.equalsIgnoreCase("RETURN")) {
      handleReturn();
    } else if (line.startsWith("AMALG")) {
      handleAmalg(line); // Adiciona suporte para o comando AMALG
    } else if (line.startsWith("SOR")) { // Adicionado suporte para o comando SOR
      handleSor(line);
    } else {
      Serial.println(F("Unknown command in program."));
    }
    currentLine++;
  }
  Serial.println(F("Program end."));
}

// Função para listar o programa
void listProgram() {
  for (int i = 0; i < lineCount; i++) {
    Serial.print(program[i].lineNumber);
    Serial.print(F(" "));
    Serial.println(program[i].command);
  }
}

// Função para criar um novo programa
void newProgram() {
  lineCount = 0;
  varCount = 0;
  Serial.println(F("New program started."));
}

// Função para adicionar uma linha ao programa
void addProgramLine(String input) {
  int spacePos = input.indexOf(' ');
  if (spacePos == -1) {
    Serial.println(F("Invalid line format."));
    return;
  }
  int lineNumber = input.substring(0, spacePos).toInt();
  String command = input.substring(spacePos + 1);

  for (int i = 0; i < lineCount; i++) {
    if (program[i].lineNumber == lineNumber) {
      program[i].command = command;
      Serial.print(F("Line "));
      Serial.print(lineNumber);
      Serial.print(F(" updated: "));
      Serial.println(command);
      return;
    }
  }

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

// Função para editar uma linha do programa
void handleEdit(String input) {
  int spacePos = input.indexOf(' ');
  if (spacePos == -1) {
    Serial.println(F("Invalid syntax for EDIT."));
    return;
  }
  int lineNumber = input.substring(5, spacePos).toInt();
  String newCommand = input.substring(spacePos + 1);

  for (int i = 0; i < lineCount; i++) {
    if (program[i].lineNumber == lineNumber) {
      program[i].command = newCommand;
      Serial.print(F("Line "));
      Serial.print(lineNumber);
      Serial.print(F(" edited: "));
      Serial.println(newCommand);
      return;
    }
  }
  Serial.println(F("Line not found."));
}

// Função para limpar a tela
void handleCls() {
  for (int i = 0; i < 50; i++) {
    Serial.println();
  }
}

// Função para exibir ajuda
void handleHelp() {
  Serial.println(F("Available commands:"));
  Serial.println(F("------------------HELP------------------"));
  Serial.println(F("HELP                          - Display this help message."));
  Serial.println(F("------------------BASIC------------------"));
  Serial.println(F("PRINT <expression>            - Print the result of an expression."));
  Serial.println(F("LET <var> = <expression>      - Assign a value to a variable."));
  Serial.println(F("GOTO <line>                   - Jump to a specific line number."));
  Serial.println(F("IF <condition> THEN <line>    - Conditional jump."));
  Serial.println(F("RUN                           - Execute the program."));
  Serial.println(F("LIST                          - List all program lines."));
  Serial.println(F("NEW                           - Clear the program."));
  Serial.println(F("END                           - End program execution."));
  Serial.println(F("CLS                           - Clear the screen."));
  Serial.println(F("PAUSE <time>                  - Pause execution for <time> milliseconds."));
  Serial.println(F("DELETE <line>                 - Delete a specific line."));
  Serial.println(F("CLEAR                         - Clear all variables and program lines."));
  Serial.println(F("------------------EXPERT------------------"));
  Serial.println(F("EDIT <line> <command>         - Edit a specific line."));
  Serial.println(F("REM <comment>                 - Add a comment."));
  Serial.println(F("RENUM                         - Renumber program lines."));
  Serial.println(F("------------------FILE MANAGEMENT------------------"));
  Serial.println(F("DIR                           - List files and directories in the current directory."));
  Serial.println(F("CD <directory>                - Change the current directory"));
  Serial.println(F("PWD                           - Print the current directory"));
  Serial.println(F("RM                            - Delete files on the SD card."));
  Serial.println(F("LOAD <filename>               - Load a program from the SD card."));
  Serial.println(F("SAVE <filename>               - Save the program to the SD card."));
  Serial.println(F("-----------------------MATH------------------------"));
  Serial.println(F("LOG <value>                   - Calculate the base-10 logarithm of a value."));
  Serial.println(F("SQR <value>                   - Calculate the square of a value."));
  Serial.println(F("RANDOM <Variable> <MIN> <MAX> - Create a variable that is randomized."));
  Serial.println(F("GRID <width> <height>         - Create a Grid."));
  Serial.println(F("VOID <width> <height>         - Create a grid of spaces."));
  Serial.println(F("RETURN                        - Go to the next line."));
  Serial.println(F("ROTATE                        - Rotate 90 degrees the last GRID or VOID."));
  Serial.println(F("SVARG <name>                  - Show the contents of a VAR  GRID, VARGRID or VARVOID."));
  Serial.println(F("AMALG <name> <type>           - Create a Amalgamate of VAR, GRID, VARGRID or VARVOID."));
  Serial.println(F("SOR <args>                    - Create a GRID-Like with chars."));
  Serial.println(F("---------------------------------------------------"));
  Serial.println(F("End of Help, have a good day!"));
}

// Função para lidar com o comando PAUSE
void handlePause(String input) {
  input = input.substring(6);
  input.trim();
  int pauseTime = input.toInt();

  if (pauseTime > 0) {
    delay(pauseTime);
  } else {
    Serial.println(F("Invalid PAUSE time."));
  }
}

// Função para lidar com o comando RENUM
void handleRenum() {
  if (lineCount == 0) {
    Serial.println(F("No program lines to renumber."));
    return;
  }

  int newLineNumber = 10;
  for (int i = 0; i < lineCount; i++) {
    program[i].lineNumber = newLineNumber;
    newLineNumber += 10;
  }

  Serial.println(F("Program lines renumbered."));
}

// Função para lidar com o comando DELETE
void handleDelete(String input) {
  input = input.substring(7);
  input.trim();
  int lineNumber = input.toInt();

  for (int i = 0; i < lineCount; i++) {
    if (program[i].lineNumber == lineNumber) {
      for (int j = i; j < lineCount - 1; j++) {
        program[j] = program[j + 1];
      }
      lineCount--;
      Serial.print(F("Line "));
      Serial.print(lineNumber);
      Serial.println(F(" deleted."));
      return;
    }
  }

  Serial.println(F("Line not found."));
}

// Função para lidar com o comando RANDOM
void handleRandom(String input) {
  input = input.substring(7);
  input.trim();

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

  float randomValue = random(0, 1000000) / 1000000.0;
  randomValue = minValue + randomValue * (maxValue - minValue);

  for (int i = 0; i < varCount; i++) {
    if (strcmp(variables[i], varName.c_str()) == 0) {
      values[i] = randomValue;
      Serial.print(F("Variable "));
      Serial.print(varName);
      Serial.print(F(" updated to: "));
      Serial.println(randomValue, 6);
      return;
    }
  }

  if (varCount < 10) {
    strcpy(variables[varCount], varName.c_str());
    values[varCount] = randomValue;
    varCount++;
    Serial.print(F("Variable "));
    Serial.print(varName);
    Serial.print(F(" created with value: "));
    Serial.println(randomValue, 6);
  } else {
    Serial.println(F("Too many variables."));
  }
}

// Função para lidar com o comando CLEAR
void handleClear() {
  varCount = 0;
  lineCount = 0;
  Serial.println(F("Variables and program cleared."));
}

// Função para lidar com o comando LOAD
void handleLoad(String input) {
  input = input.substring(5);
  input.trim();
  String filename = input + ".txt";

  File file = SD.open(filename);
  if (!file) {
    Serial.println(F("File not found."));
    return;
  }

  newProgram();

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    addProgramLine(line);
  }

  file.close();
  Serial.println(F("Program loaded from SD card."));
}

// Função para lidar com o comando ASK
void handleAsk(String input) {
  input = input.substring(4);
  input.trim();

  if (input.length() == 0) {
    Serial.println(F("Invalid syntax for ASK. Use: ASK <variable>"));
    return;
  }

  while (!Serial.available()) {
    // Espera até que o usuário insira um valor
  }
  String valueStr = Serial.readStringUntil('\n');
  valueStr.trim();

  float value = valueStr.toFloat();

  for (int i = 0; i < varCount; i++) {
    if (strcmp(variables[i], input.c_str()) == 0) {
      values[i] = value;
      Serial.print(F("Variable "));
      Serial.print(input);
      Serial.print(F(" updated to: "));
      Serial.println(value, 6);
      return;
    }
  }

  if (varCount < 10) {
    strcpy(variables[varCount], input.c_str());
    values[varCount] = value;
    varCount++;
  } else {
    Serial.println(F("Too many variables."));
  }
}

// Função para rotacionar o grid em 90 graus
void rotateGrid() {
  if (gridWidth == 0 || gridHeight == 0) {
    Serial.println(F("Error: No grid to rotate."));
    return;
  }

  // Cria uma matriz temporária para armazenar o grid rotacionado
  char rotatedGrid[MAX_GRID_SIZE][MAX_GRID_SIZE];

  // Rotaciona o grid
  for (int i = 0; i < gridHeight; i++) {
    for (int j = 0; j < gridWidth; j++) {
      rotatedGrid[j][gridHeight - 1 - i] = grid[i][j];
    }
  }

  // Atualiza as dimensões do grid
  int temp = gridWidth;
  gridWidth = gridHeight;
  gridHeight = temp;

  // Copia o grid rotacionado de volta para a matriz original
  for (int i = 0; i < gridHeight; i++) {
    for (int j = 0; j < gridWidth; j++) {
      grid[i][j] = rotatedGrid[i][j];
    }
  }

  Serial.println(F("Grid rotated 90 degrees."));
}

// Função para exibir o grid armazenado
void displayGrid() {
  if (gridWidth == 0 || gridHeight == 0) {
    Serial.println(F("Error: No grid to display."));
    return;
  }

  for (int i = 0; i < gridHeight; i++) {
    for (int j = 0; j < gridWidth; j++) {
      Serial.print('[');
      Serial.print(grid[i][j]);
      Serial.print(']');
    }
    Serial.println();
  }
}

// Função para lidar com o comando ROTATE
void handleRotate() {
  rotateGrid();
  displayGrid();
}

// Função para lidar com o comando GRID
void handleGrid(String input) {
  input = input.substring(5); // Remove "GRID "
  input.trim();

  int spacePos = input.indexOf(' ');
  if (spacePos == -1) {
    Serial.println(F("Error: Invalid syntax. Correct: GRID <width> <height>."));
    return;
  }

  String widthStr = input.substring(0, spacePos);
  String heightStr = input.substring(spacePos + 1);

  gridWidth = widthStr.toInt();
  gridHeight = heightStr.toInt();

  if (gridWidth <= 0 || gridHeight <= 0 || gridWidth > MAX_GRID_SIZE || gridHeight > MAX_GRID_SIZE) {
    Serial.println(F("Error: Invalid grid dimensions."));
    return;
  }

  // Preenche o grid com o padrão '#'
  for (int i = 0; i < gridHeight; i++) {
    for (int j = 0; j < gridWidth; j++) {
      grid[i][j] = '#';
    }
  }

  // Exibe o grid sem nova linha no final
  for (int i = 0; i < gridHeight; i++) {
    for (int j = 0; j < gridWidth; j++) {
      Serial.print('[');
      Serial.print(grid[i][j]);
      Serial.print(']');
    }
    if (i < gridHeight - 1) {
      Serial.println(); // Nova linha apenas entre as linhas do grid
    }
  }
}

// Função para lidar com o comando RETURN
void handleReturn() {
  Serial.println(); // Simplesmente imprime uma nova linha
}

// Função para lidar com o comando RM
void handleRm(String input) {
  input = input.substring(3);
  input.trim();

  if (input.length() == 0 || input.indexOf('/') != -1 || input.indexOf('\\') != -1) {
    Serial.println(F("Invalid filename."));
    return;
  }

  String fullPath = currentDir + input + ".txt";

  if (!SD.exists(fullPath)) {
    Serial.println(F("File not found."));
    return;
  }

  if (SD.remove(fullPath)) {
    Serial.print(F("File "));
    Serial.print(input);
    Serial.println(F(".txt deleted."));
  } else {
    Serial.println(F("Failed to delete file."));
  }
}

// Função para lidar com o comando VOID
void handleVoid(String input) {
  input = input.substring(5); // Remove "VOID "
  input.trim();

  int spacePos = input.indexOf(' ');
  if (spacePos == -1) {
    Serial.println(F("Error: Invalid syntax. Correct: VOID <width> <height>"));
    return;
  }

  String widthStr = input.substring(0, spacePos);
  String heightStr = input.substring(spacePos + 1);

  gridWidth = widthStr.toInt();
  gridHeight = heightStr.toInt();

  if (gridWidth <= 0 || gridHeight <= 0 || gridWidth > MAX_GRID_SIZE || gridHeight > MAX_GRID_SIZE) {
    Serial.println(F("Error: Invalid grid dimensions."));
    return;
  }

  // Preenche o grid com espaços em branco
  for (int i = 0; i < gridHeight; i++) {
    for (int j = 0; j < gridWidth; j++) {
      grid[i][j] = ' ';
    }
  }

  // Exibe o grid sem nova linha no final
  for (int i = 0; i < gridHeight; i++) {
    for (int j = 0; j < gridWidth; j++) {
      Serial.print('[');
      Serial.print(grid[i][j]);
      Serial.print(']');
    }
    if (i < gridHeight - 1) {
      Serial.println(); // Nova linha apenas entre as linhas do grid
    }
  }
}
// Modificação na função handleEnter para exibir o grid
void handleEnter() {
  // Exibe o grid sem nova linha no final
  for (int i = 0; i < gridHeight; i++) {
    for (int j = 0; j < gridWidth; j++) {
      Serial.print('[');
      Serial.print(grid[i][j]);
      Serial.print(']');
    }
    if (i < gridHeight - 1) {
      Serial.println(); // Nova linha apenas entre as linhas do grid
    }
  }
}



// Função para lidar com o comando SAVE
void handleSave(String input) {
  input = input.substring(5);
  input.trim();
  String filename = input + ".txt";

  if (filename.length() == 0 || filename.indexOf('/') != -1 || filename.indexOf('\\') != -1) {
    Serial.println(F("Invalid filename."));
    return;
  }

  String fullPath = currentDir + "/" + filename;

  if (SD.exists(fullPath)) {
    if (!SD.remove(fullPath)) {
      Serial.println(F("Failed to remove existing file."));
      return;
    }
  }

  File file = SD.open(fullPath, FILE_WRITE);
  if (!file) {
    Serial.println(F("Error creating file. Check SD card."));
    return;
  }

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

// Função de configuração
void setup() {
  Serial.begin(9600);
  Serial.println(F("------------Arduino BASIC v0.6a-----------"));

  if (!SD.begin(10)) {
    Serial.println(F("SD card initialization failed!"));
    return;
  }
  Serial.println(F("SD card initialized."));
}

// Function to rotate an AMALG variable


// Função principal
void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.startsWith("SOR")) {
      handleSor(input); // Executa o comando SOR
    } else if (input.startsWith("AMALG")) {
      handleAmalg(input); // Executa o comando AMALG
    } else if (input.startsWith("ROTATE ")) {
      handleRotateVar(input); // Rotaciona uma variável AMALG ou GRID
    } else if (input.equalsIgnoreCase("RETURN")) {
      handleReturn();
    } else if (input.startsWith("VARGRID")) {
      handleVarGrid(input);
    } else if (input.startsWith("SVARG")) {
      handleSvarg(input);
    } else if (input.startsWith("VARVOID")) {
      handleVarVoid(input);
    } else if (input.startsWith("VARENTER")) {
      handleVarEnter(input);
    } else if (input.equalsIgnoreCase("ENTER")) {
      handleEnter();
    } else if (input.equalsIgnoreCase("ROTATE")) {
      handleRotate();
    } else if (input.startsWith("VOID")) {
      handleVoid(input);
    } else if (input.startsWith("HELP")) {
      handleHelp();
    } else if (input.startsWith("GRID")) {
      handleGrid(input);
    } else if (input.startsWith("PRINT")) {
      handlePrint(input);
    } else if (input.startsWith("LET")) {
      handleLet(input);
    } else if (input.startsWith("GOTO")) {
      handleGoto(input);
    } else if (input.startsWith("ASK")) {
      handleAsk(input);
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
      return;
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
    } else if (input.startsWith("PWD")) {
      handlePwd();
    } else if (input.startsWith("DIR")) {
      handleDir();
    } else if (input.startsWith("LOG")) {
      handleLog(input);
    } else if (input.startsWith("SQRT")) {
      handleSqrt(input);
    } else if (isdigit(input.charAt(0))) {
      addProgramLine(input);
    } else {
      Serial.println(F("Unknown command. Type HELP for a list of commands."));
    }
  }
}
